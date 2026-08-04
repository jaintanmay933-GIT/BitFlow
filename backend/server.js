const express = require('express');
const cors = require('cors');
const amqp = require('amqplib');
const grpc = require('@grpc/grpc-js');
const protoLoader = require('@grpc/proto-loader');
const { Pool } = require('pg');
const path = require('path');
const multer = require('multer');
const fs = require('fs');

const app = express();

// Enable CORS for all incoming requests (Vercel frontend)
app.use(cors());
app.use(express.json());

// ==========================================
// CONFIGURATION & PATHS
// ==========================================
const HTTP_PORT = process.env.PORT || 5000;
const GRPC_PORT = process.env.GRPC_PORT || 50051;
const RABBITMQ_URL = process.env.RABBITMQ_URL || 'amqp://127.0.0.1:5672';
const BACKEND_URL = process.env.BACKEND_URL || 'https://bitflow-backend-047r.onrender.com';
const QUEUE_NAME = 'video_jobs';

// Absolute paths to Shared Storage directories
const INPUT_DIR = path.resolve(__dirname, '../shared_storage/inputs');
const OUTPUT_DIR = path.resolve(__dirname, '../shared_storage/outputs');

// Ensure storage directories exist at startup
if (!fs.existsSync(INPUT_DIR)) fs.mkdirSync(INPUT_DIR, { recursive: true });
if (!fs.existsSync(OUTPUT_DIR)) fs.mkdirSync(OUTPUT_DIR, { recursive: true });

// Expose inputs directory so the C++ worker container can fetch uploaded files via HTTP
app.use('/inputs', express.static(INPUT_DIR));

// Expose static stream directory for processed MP4 video playback on frontend
app.use('/stream', express.static(OUTPUT_DIR));

// ==========================================
// MULTER FILE UPLOAD CONFIGURATION
// ==========================================
const storage = multer.diskStorage({
    destination: (req, file, cb) => cb(null, INPUT_DIR),
    filename: (req, file, cb) => {
        const uniquePrefix = Date.now() + '-' + Math.round(Math.random() * 1E9);
        cb(null, uniquePrefix + path.extname(file.originalname));
    }
});
const upload = multer({ storage: storage });

// ==========================================
// DATABASE (POSTGRESQL / NEON) POOL
// ==========================================
const dbConfig = process.env.DATABASE_URL
    ? {
        connectionString: process.env.DATABASE_URL,
        ssl: { rejectUnauthorized: false }
      }
    : {
        user: process.env.DB_USER || 'root',
        host: process.env.DB_HOST || '127.0.0.1',
        database: process.env.DB_NAME || 'bitflow_db',
        password: process.env.DB_PASSWORD || 'secretpassword',
        port: process.env.DB_PORT || 5432,
      };

const pool = new Pool(dbConfig);

// ==========================================
// RABBITMQ BROKER CONNECTION
// ==========================================
let channel;
async function connectRabbitMQ() {
    try {
        const connection = await amqp.connect(RABBITMQ_URL);
        
        connection.on('error', (err) => {
            console.error('❌ [RabbitMQ Error]:', err.message);
        });

        connection.on('close', () => {
            console.warn('⚠️ [RabbitMQ Connection Closed]: Retrying in 5s...');
            setTimeout(connectRabbitMQ, 5000);
        });

        channel = await connection.createChannel();
        await channel.assertQueue(QUEUE_NAME, { durable: true });
        console.log(`🚀 [RabbitMQ] Producer channel active: "${QUEUE_NAME}"`);
    } catch (err) {
        console.error('❌ [RabbitMQ Initial Connect Failed]: Retrying in 5s...', err.message);
        setTimeout(connectRabbitMQ, 5000);
    }
}
connectRabbitMQ();

// Helper: UUID v4 regex validation to prevent Postgres 22P02 syntax errors
const isUUID = (str) => /^[0-9a-f]{8}-[0-9a-f]{4}-[1-5][0-9a-f]{3}-[89ab][0-9a-f]{3}-[0-9a-f]{12}$/i.test(str);

// ==========================================
// REST API ENDPOINTS
// ==========================================

// 1. Submit Video Transcoding Job
app.post('/api/transcode', upload.single('video'), async (req, res) => {
    const { title } = req.body;

    if (!title || !req.file) {
        return res.status(400).json({ error: 'Missing title parameter or video file data' });
    }

    try {
        const originalFilename = req.file.originalname;
        const uploadedFilename = req.file.filename;

        // Create public HTTP URL so external C++ worker container can access FFmpeg stream
        const publicInputUrl = `${BACKEND_URL}/inputs/${uploadedFilename}`;

        // Insert new record into PostgreSQL
        const dbResult = await pool.query(
            `INSERT INTO videos (title, original_filename, s3_raw_key, status) 
             VALUES ($1, $2, $3, 'PENDING') RETURNING id`,
            [title, originalFilename, publicInputUrl]
        );

        const videoId = dbResult.rows[0].id;
        const finalOutputLocation = path.join(OUTPUT_DIR, `${videoId}.mp4`);

        // Job Payload consumed by C++ Worker (inputPath is an accessible HTTP URL)
        const jobPayload = {
            jobId: videoId,
            inputPath: publicInputUrl,
            outputPath: finalOutputLocation
        };

        if (channel) {
            channel.sendToQueue(QUEUE_NAME, Buffer.from(JSON.stringify(jobPayload)), { persistent: true });
            console.log(`📥 [Job Queued] Video ID: ${videoId}`);
            return res.status(202).json({ success: true, videoId });
        } else {
            throw new Error('RabbitMQ channel uninitialized');
        }
    } catch (err) {
        console.error('❌ [API /transcode Error]:', err.message);
        return res.status(500).json({ error: 'Internal system processing failure' });
    }
});

// 2. Query Transcoding Progress & Status
app.get('/api/status/:id', async (req, res) => {
    const jobId = req.params.id;

    if (!isUUID(jobId)) {
        return res.status(400).json({ error: 'Invalid UUID format provided' });
    }

    try {
        const result = await pool.query(
            'SELECT id, title, status, progress, error_message FROM videos WHERE id = $1',
            [jobId]
        );

        if (result.rows.length === 0) {
            return res.status(404).json({ error: 'Job non-existent' });
        }

        const job = result.rows[0];
        return res.json({
            success: true,
            title: job.title,
            status: job.status,
            progress: parseInt(job.progress || 0, 10),
            error: job.error_message,
            streamUrl: job.status === 'COMPLETED' ? `${BACKEND_URL}/stream/${job.id}.mp4` : null
        });
    } catch (err) {
        console.error('❌ [API /status Error]:', err.message);
        return res.status(500).json({ error: 'Internal database query failure' });
    }
});

// ==========================================
// gRPC TELEMETRY CALLBACK SERVER
// ==========================================
const protoPath = path.resolve(__dirname, '../pb/video_service.proto');
const packageDefinition = protoLoader.loadSync(protoPath, {
    keepCase: true,
    longs: String,
    enums: String,
    defaults: true,
    oneofs: true
});
const bitflowProto = grpc.loadPackageDefinition(packageDefinition).bitflow;

async function updateProgress(call, callback) {
    const job_id = call.request.job_id || call.request.jobId;
    const percentage = call.request.percentage;
    const status = call.request.status;
    const error_message = call.request.error_message || call.request.errorMessage;

    if (!job_id || !isUUID(job_id)) {
        console.error(`❌ [gRPC Error]: Invalid Job UUID received: "${job_id}"`);
        return callback({ code: grpc.status.INVALID_ARGUMENT, message: 'Invalid job UUID' });
    }

    try {
        await pool.query(
            `UPDATE videos 
             SET progress = $1, status = $2::video_status, error_message = $3, updated_at = CURRENT_TIMESTAMP 
             WHERE id = $4`,
            [percentage, status, error_message || null, job_id]
        );

        console.log(`🔄 [gRPC Telemetry] Job ${job_id} -> ${status} (${percentage}%)`);
        callback(null, { success: true });
    } catch (err) {
        console.error(`❌ [gRPC DB Update Failed]: ${err.message}`);
        callback({ code: grpc.status.INTERNAL, message: err.message });
    }
}

function startGrpcServer() {
    const grpcServer = new grpc.Server();
    grpcServer.addService(bitflowProto.BitFlowCallbackService.service, { updateProgress });

    // Bind to 0.0.0.0 so external containers/workers can connect
    grpcServer.bindAsync(`0.0.0.0:${GRPC_PORT}`, grpc.ServerCredentials.createInsecure(), (err, port) => {
        if (!err) {
            console.log(`📡 [gRPC Server] Live and listening on 0.0.0.0:${port}`);
        } else {
            console.error(`❌ [gRPC Bind Failed]: ${err.message}`);
        }
    });
}
startGrpcServer();

// ==========================================
// HTTP SERVER LIFECYCLE
// ==========================================
app.listen(HTTP_PORT, () => console.log(`🌐 [HTTP Gateway] Active on port ${HTTP_PORT}`));