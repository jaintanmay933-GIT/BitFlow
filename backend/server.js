const dns = require('dns');
dns.setDefaultResultOrder('ipv4first'); // Prevents WSL2 fetch/IPv6 timeouts

const path = require('path');
require('dotenv').config({ path: path.resolve(__dirname, '.env') });

const express = require('express');
const cors = require('cors');
const amqp = require('amqplib');
const grpc = require('@grpc/grpc-js');
const protoLoader = require('@grpc/proto-loader');
const { neon } = require('@neondatabase/serverless');
const multer = require('multer');
const fs = require('fs');

const app = express();
app.use(cors());
app.use(express.json());

// ==========================================
// CONFIGURATION & PATHS
// ==========================================
const HTTP_PORT = process.env.PORT || 5000;
const GRPC_PORT = process.env.GRPC_PORT || 50051;
const RABBITMQ_URL = process.env.RABBITMQ_URL || 'amqp://127.0.0.1:5672';
const QUEUE_NAME = 'video_jobs';
const BASE_URL = process.env.BASE_URL || `http://localhost:${HTTP_PORT}`;

// Storage directories
const INPUT_DIR = path.resolve(__dirname, '../shared_storage/inputs');
const OUTPUT_DIR = path.resolve(__dirname, '../shared_storage/outputs');

if (!fs.existsSync(INPUT_DIR)) fs.mkdirSync(INPUT_DIR, { recursive: true });
if (!fs.existsSync(OUTPUT_DIR)) fs.mkdirSync(OUTPUT_DIR, { recursive: true });

app.use('/stream', express.static(OUTPUT_DIR));

// ==========================================
// MULTER FILE UPLOAD
// ==========================================
const storage = multer.diskStorage({
  destination: (req, file, cb) => cb(null, INPUT_DIR),
  filename: (req, file, cb) => {
    const uniquePrefix = Date.now() + '-' + Math.round(Math.random() * 1e9);
    cb(null, uniquePrefix + path.extname(file.originalname));
  },
});
const upload = multer({ storage });

// ==========================================
// NEON POSTGRESQL DRIVER
// ==========================================
const sql = neon(process.env.DATABASE_URL);

// Startup connection check
(async () => {
  try {
    const result = await sql`SELECT version()`;
    console.log('🐘 [PostgreSQL] Connected to Neon Cloud Database successfully!');
    console.log('ℹ️  Engine Version:', result[0].version.split(' ')[0]);
  } catch (err) {
    console.error('❌ [PostgreSQL Connection Error]:', err.cause || err.message || err);
  }
})();

// ==========================================
// RABBITMQ CONNECTION
// ==========================================
let channel;
async function connectRabbitMQ() {
  try {
    const connection = await amqp.connect(RABBITMQ_URL);

    connection.on('error', (err) => console.error('❌ [RabbitMQ Error]:', err.message));
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

const isUUID = (str) =>
  /^[0-9a-f]{8}-[0-9a-f]{4}-[1-5][0-9a-f]{3}-[89ab][0-9a-f]{3}-[0-9a-f]{12}$/i.test(str);

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
    const realInputLocation = req.file.path;

    // Neon Tagged Template Insert Syntax
    const dbResult = await sql`
      INSERT INTO videos (title, original_filename, s3_raw_key, status)
      VALUES (${title}, ${originalFilename}, ${realInputLocation}, 'PENDING')
      RETURNING id
    `;

    const videoId = dbResult[0].id;
    const finalOutputLocation = path.join(OUTPUT_DIR, `${videoId}.mp4`);

    const jobPayload = {
      jobId: videoId,
      inputPath: realInputLocation,
      outputPath: finalOutputLocation,
    };

    if (channel) {
      channel.sendToQueue(QUEUE_NAME, Buffer.from(JSON.stringify(jobPayload)), { persistent: true });
      console.log(`📥 [Job Queued] Video ID: ${videoId}`);
      return res.status(202).json({ success: true, videoId });
    } else {
      throw new Error('RabbitMQ channel uninitialized');
    }
  } catch (err) {
    console.error('❌ [API /transcode Error]:', err.cause || err.message || err);
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
    // Neon Tagged Template Select Syntax
    const result = await sql`
      SELECT id, title, status, progress, error_message 
      FROM videos 
      WHERE id = ${jobId}
    `;

    if (result.length === 0) {
      return res.status(404).json({ error: 'Job non-existent' });
    }

    const job = result[0];
    return res.json({
      success: true,
      title: job.title,
      status: job.status,
      progress: parseInt(job.progress || 0, 10),
      error: job.error_message,
      streamUrl: job.status === 'COMPLETED' ? `${BASE_URL}/stream/${job.id}.mp4` : null,
    });
  } catch (err) {
    console.error('❌ [API /status Error]:', err.cause || err.message || err);
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
  oneofs: true,
});
const bitflowProto = grpc.loadPackageDefinition(packageDefinition).bitflow;

async function updateProgress(call, callback) {
  const job_id = call.request.job_id || call.request.jobId;
  const rawPercentage = call.request.percentage;
  const status = call.request.status;
  const error_message = call.request.error_message || call.request.errorMessage;

  if (!job_id || !isUUID(job_id)) {
    console.error(`❌ [gRPC Error]: Invalid Job UUID received: "${job_id}"`);
    return callback({ code: grpc.status.INVALID_ARGUMENT, message: 'Invalid job UUID' });
  }

  const percentage = Math.min(100, Math.max(0, parseInt(rawPercentage || 0, 10)));

  try {
    // Neon Tagged Template Update Syntax
    await sql`
      UPDATE videos 
      SET progress = ${percentage}, 
          status = ${status}::video_status, 
          error_message = ${error_message || null}, 
          updated_at = CURRENT_TIMESTAMP 
      WHERE id = ${job_id}
    `;

    console.log(`🔄 [gRPC Telemetry] Job ${job_id} -> ${status} (${percentage}%)`);
    callback(null, { success: true });
  } catch (err) {
    console.error(`❌ [gRPC DB Update Failed]: ${err.cause || err.message || err}`);
    callback({ code: grpc.status.INTERNAL, message: err.message });
  }
}

function startGrpcServer() {
  const grpcServer = new grpc.Server();
  grpcServer.addService(bitflowProto.BitFlowCallbackService.service, { updateProgress });

  grpcServer.bindAsync(`127.0.0.1:${GRPC_PORT}`, grpc.ServerCredentials.createInsecure(), (err, port) => {
    if (!err) {
      console.log(`📡 [gRPC Server] Live and listening on port :${port}`);
    } else {
      console.error(`❌ [gRPC Bind Failed]: ${err.message}`);
    }
  });
}
startGrpcServer();

// ==========================================
// HTTP SERVER LIFECYCLE
// ==========================================
app.listen(HTTP_PORT, () => console.log(`🌐 [HTTP Gateway] Active on http://localhost:${HTTP_PORT}`));