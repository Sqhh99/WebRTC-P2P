-- 消息表
CREATE TABLE IF NOT EXISTS message (
    id BIGINT PRIMARY KEY AUTO_INCREMENT COMMENT '主键ID',
    conversation VARCHAR(100) NOT NULL COMMENT '会话ID',
    sender_id VARCHAR(50) NOT NULL COMMENT '发送者ID',
    timestamp TIMESTAMP NOT NULL COMMENT '消息时间戳',
    type VARCHAR(20) NOT NULL COMMENT '消息类型 text/image/file/emoji',
    content TEXT COMMENT '消息内容',
    extra JSON COMMENT '额外数据',
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP COMMENT '创建时间',
    INDEX idx_conversation_time (conversation, timestamp),
    INDEX idx_sender (sender_id),
    INDEX idx_timestamp (timestamp)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='消息表';

-- 会话列表表
CREATE TABLE IF NOT EXISTS conversation (
    id BIGINT PRIMARY KEY AUTO_INCREMENT COMMENT '主键ID',
    conversation_id VARCHAR(100) NOT NULL COMMENT '会话ID',
    user_id VARCHAR(50) NOT NULL COMMENT '用户ID',
    target_id VARCHAR(50) NOT NULL COMMENT '对方ID(用户或群组)',
    is_group BOOLEAN DEFAULT FALSE COMMENT '是否群聊',
    last_message TEXT COMMENT '最后一条消息',
    last_message_time TIMESTAMP NULL COMMENT '最后消息时间',
    unread_count INT DEFAULT 0 COMMENT '未读消息数',
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP COMMENT '创建时间',
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP COMMENT '更新时间',
    INDEX idx_user (user_id, updated_at),
    UNIQUE KEY uk_user_conversation (user_id, conversation_id)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='会话列表表';

-- 离线消息索引表
CREATE TABLE IF NOT EXISTS offline_message_index (
    id BIGINT PRIMARY KEY AUTO_INCREMENT COMMENT '主键ID',
    user_id VARCHAR(50) NOT NULL COMMENT '用户ID',
    message_id BIGINT NOT NULL COMMENT '消息ID',
    conversation_id VARCHAR(100) NOT NULL COMMENT '会话ID',
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP COMMENT '创建时间',
    synced BOOLEAN DEFAULT FALSE COMMENT '是否已同步',
    synced_at TIMESTAMP NULL COMMENT '同步时间',
    INDEX idx_user_synced (user_id, synced, created_at),
    INDEX idx_message (message_id),
    INDEX idx_conversation (conversation_id)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='离线消息索引表';

-- 推送日志表
CREATE TABLE IF NOT EXISTS push_log (
    id BIGINT PRIMARY KEY AUTO_INCREMENT COMMENT '主键ID',
    user_id VARCHAR(50) NOT NULL COMMENT '用户ID',
    message_id BIGINT NOT NULL COMMENT '消息ID',
    push_type VARCHAR(20) NOT NULL COMMENT '推送类型 apns/fcm/email',
    push_status VARCHAR(20) NOT NULL COMMENT '推送状态 pending/success/failed',
    push_time TIMESTAMP NULL COMMENT '推送时间',
    error_msg TEXT COMMENT '错误信息',
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP COMMENT '创建时间',
    INDEX idx_user_status (user_id, push_status),
    INDEX idx_message (message_id),
    INDEX idx_created (created_at)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='推送日志表';
