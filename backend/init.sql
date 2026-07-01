-- Create an isolated enum tracker for our system pipeline states
CREATE TYPE video_status AS ENUM ('PENDING', 'PROCESSING', 'COMPLETED', 'FAILED');

-- Core videos ledger table
CREATE TABLE IF NOT EXISTS videos (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    title VARCHAR(255) NOT NULL,
    original_filename VARCHAR(255) NOT NULL,
    s3_raw_key VARCHAR(512) NOT NULL,
    s3_processed_key VARCHAR(512),
    status video_status DEFAULT 'PENDING',
    progress INT DEFAULT 0 CHECK (progress >= 0 AND progress <= 100),
    error_message TEXT,
    created_at TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP
);

-- Index critical lookups so updates from our C++ engine are instantaneous
CREATE INDEX IF NOT EXISTS idx_videos_status ON videos(status);