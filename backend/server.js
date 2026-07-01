const express = require('express');
const cors = require('cors');
const amqp = require('amqplib');
const grpc = require('@grpc/grpc-js');
const protoLoader = require('@grpc/proto-loader');
const { Pool } = require('pg');
const path = require('path');

const app = express();
app.use(cors());
app.use(express.json());

const HTTP_PORT = 5000;
const GRPC_PORT = 50051;
const RABBITMQ_URL = 'amqp://localhost:5672';
const QUEUE_NAME = 'video_jobs';

// 1. DATABASE: PostgreSQL Connection Setup
const pool = new Pool({
    user: 'root',
    host: 'localhost',
    database: 'bitflow_db',
    password: 'secretpassword',
    port: 5432,
});

// 2. RABBITMQ: Producer Pipeline Connection
let channel;
async function connectRabbitMQ() {
    try {
        const connection = await amqp.connect(RABBITMQ_URL);
        channel = await connection.createChannel();
        await channel.assertQueue(QUEUE_NAME, { durable: true });
        console.log(`🚀 [RabbitMQ] Connected and listening to queue: "${QUEUE_NAME}"`);
    } catch (err) {
        console.error('❌ [RabbitMQ] Connection failed! Retrying in 5s...', err.message);
        setTimeout(connectRabbitMQ, 5000);
    }
}
connectRabbitMQ();

// Enhanced HTTP Endpoint: Save entry to Postgres and publish to Queue
app.post('/api/transcode', async (req, res) => {
    const { title, filename } = req.body;
    
    if (!title || !filename) {
        return res.status(400).json({ error: 'Missing title or filename parameters' });
    }

    try {
        // Generate pseudo-S3 path variables for our storage engine
        const s3_raw_key = `uploads/raw/${filename}`;

        // Insert video job state ledger entry into PostgreSQL database
        const dbResult = await pool.query(
            `INSERT INTO videos (title, original_filename, s3_raw_key, status) 
             VALUES ($1, $2, $3, 'PENDING') RETURNING id`,
            [title, filename, s3_raw_key]
        );
        
        const videoId = dbResult.rows[0].id;

        const jobPayload = {
            jobId: videoId,
            inputPath: s3_raw_key,
            outputPath: `uploads/processed/${videoId}/`
        };

        // Forward task payload over to RabbitMQ queue
        if (channel) {
            channel.sendToQueue(QUEUE_NAME, Buffer.from(JSON.stringify(jobPayload)), { persistent: true });
            console.log(`📥 [Job Queued] Persistent DB Record Created for ID: ${videoId}`);
            return res.status(202).json({ success: true, videoId, message: 'Job initialized successfully' });
        } else {
            throw new Error('RabbitMQ pipeline agent offline');
        }
    } catch (err) {
        console.error('❌ [API Error]:', err.message);
        return res.status(500).json({ error: 'Internal server initialization failure' });
    }
});

// 3. gRPC: Live Engine Callback Listener (Updates Postgres Status)
const protoPath = path.join(__dirname, '../pb/video_service.proto');
const packageDefinition = protoLoader.loadSync(protoPath, { keepCase: true, longs: String, enums: String, defaults: true, oneofs: true });
const bitflowProto = grpc.loadPackageDefinition(packageDefinition).bitflow;

async function updateProgress(call, callback) {
    const { job_id, percentage, status, error_message } = call.request;
    
    try {
        await pool.query(
            `UPDATE videos 
             SET progress = $1, status = $2::video_status, error_message = $3, updated_at = CURRENT_TIMESTAMP 
             WHERE id = $4`,
            [percentage, status, error_message || null, job_id]
        );
        console.log(`🔄 [gRPC DB Update] Job ID: ${job_id} modified to ${status} (${percentage}%)`);
        callback(null, { success: true });
    } catch (err) {
        console.error('❌ [gRPC DB Update Failed]:', err.message);
        callback({ code: grpc.status.INTERNAL, message: 'Database state alteration rejected' });
    }
}

function startGrpcServer() {
    const grpcServer = new grpc.Server();
    grpcServer.addService(bitflowProto.BitFlowCallbackService.service, { updateProgress });
    grpcServer.bindAsync(`0.0.0.0:${GRPC_PORT}`, grpc.ServerCredentials.createInsecure(), (err, port) => {
        if (!err) console.log(`📡 [gRPC Server] Live and listening on port :${port}`);
    });
}
startGrpcServer();

app.listen(HTTP_PORT, () => {
    console.log(`🌐 [HTTP Gateway] API listening on http://localhost:${HTTP_PORT}`);
});