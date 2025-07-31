// ===== websocket_bridge.js =====
// This Node.js server bridges WebSocket clients to your TCP server
// Run with: node websocket_bridge.js

const WebSocket = require('ws');
const net = require('net');

const WS_PORT = 8889; // WebSocket server port
const TCP_HOST = '127.0.0.1';
const TCP_PORT = 8888; // Your C++ server port

console.log('🌉 Starting WebSocket Bridge Server...');

const wss = new WebSocket.Server({
    port: WS_PORT,
    perMessageDeflate: false
});

console.log(`📡 WebSocket server listening on port ${WS_PORT}`);
console.log(`🔗 Will bridge to TCP server at ${TCP_HOST}:${TCP_PORT}`);

wss.on('connection', (ws) => {
    console.log('🔌 New WebSocket client connected');
    
    let tcpSocket = null;
    let isConnected = false;
    let frameBuffer = Buffer.alloc(0);
    let expectedFrameSize = 0;
    let frameHeaderReceived = false;
    
    // Connect to TCP server
    tcpSocket = new net.Socket();
    
    tcpSocket.connect(TCP_PORT, TCP_HOST, () => {
        console.log('✅ Connected to TCP server');
        isConnected = true;
        
        // Send connection confirmation to web client
        ws.send(JSON.stringify({
            type: 'connected',
            message: 'Successfully connected to remote desktop'
        }));
    });
    
    // Handle data from TCP server (screen frames)
    tcpSocket.on('data', (data) => {
        try {
            frameBuffer = Buffer.concat([frameBuffer, data]);
            
            // Process complete frames
            while (frameBuffer.length > 0) {
                if (!frameHeaderReceived) {
                    // Need at least 12 bytes for frame header (4+4+4)
                    if (frameBuffer.length < 12) break;
                    
                    const dataSize = frameBuffer.readUInt32LE(0);
                    const width = frameBuffer.readUInt32LE(4);
                    const height = frameBuffer.readUInt32LE(8);
                    
                    expectedFrameSize = dataSize;
                    frameHeaderReceived = true;
                    frameBuffer = frameBuffer.slice(12);
                    
                    console.log(`📸 Expecting frame: ${width}x${height}, ${dataSize} bytes`);
                }
                
                if (frameHeaderReceived) {
                    if (frameBuffer.length >= expectedFrameSize) {
                        // We have a complete frame
                        const frameData = frameBuffer.slice(0, expectedFrameSize);
                        frameBuffer = frameBuffer.slice(expectedFrameSize);
                        
                        // Send frame to WebSocket client as binary data
                        if (ws.readyState === WebSocket.OPEN) {
                            ws.send(frameData, { binary: true });
                        }
                        
                        // Reset for next frame
                        frameHeaderReceived = false;
                        expectedFrameSize = 0;
                    } else {
                        // Wait for more data
                        break;
                    }
                }
            }
        } catch (error) {
            console.error('❌ Error processing frame data:', error);
        }
    });
    
    // Handle WebSocket messages from client (input events)
    ws.on('message', (message) => {
        if (!isConnected || !tcpSocket) return;
        
        try {
            const event = JSON.parse(message);
            
            if (event.eventType === 1) { // Mouse event
                const mouseData = Buffer.alloc(6);
                mouseData.writeUInt8(1, 0); // Event type: mouse
                mouseData.writeUInt8(event.type, 1); // Mouse event type
                mouseData.writeInt16LE(event.x, 2); // X coordinate
                mouseData.writeInt16LE(event.y, 4); // Y coordinate
                
                tcpSocket.write(mouseData);
                
            } else if (event.eventType === 2) { // Keyboard event
                const keyData = Buffer.alloc(8);
                keyData.writeUInt8(2, 0); // Event type: keyboard
                keyData.writeUInt8(event.type, 1); // Key event type
                keyData.writeUInt16LE(event.keyCode, 2); // Key code
                keyData.writeUInt32LE(0, 4); // Flags
                
                tcpSocket.write(keyData);
            }
            
        } catch (error) {
            console.error('❌ Error processing WebSocket message:', error);
        }
    });
    
    // Handle TCP socket errors
    tcpSocket.on('error', (error) => {
        console.error('❌ TCP socket error:', error);
        if (ws.readyState === WebSocket.OPEN) {
            ws.send(JSON.stringify({
                type: 'error',
                message: 'Connection to remote desktop lost'
            }));
        }
    });
    
    // Handle TCP socket close
    tcpSocket.on('close', () => {
        console.log('🔌 TCP connection closed');
        isConnected = false;
        if (ws.readyState === WebSocket.OPEN) {
            ws.send(JSON.stringify({
                type: 'disconnected',
                message: 'Remote desktop connection closed'
            }));
        }
    });
    
    // Handle WebSocket close
    ws.on('close', () => {
        console.log('🔌 WebSocket client disconnected');
        if (tcpSocket) {
            tcpSocket.destroy();
        }
    });
    
    // Handle WebSocket errors
    ws.on('error', (error) => {
        console.error('❌ WebSocket error:', error);
        if (tcpSocket) {
            tcpSocket.destroy();
        }
    });
});

// Handle server errors
wss.on('error', (error) => {
    console.error('❌ WebSocket server error:', error);
});

// Graceful shutdown
process.on('SIGINT', () => {
    console.log('\n🛑 Shutting down WebSocket bridge...');
    wss.close(() => {
        console.log('✅ WebSocket server closed');
        process.exit(0);
    });
});

console.log('🚀 WebSocket Bridge Server is ready!');
console.log('📝 Usage:');
console.log('  1. Start your C++ remote desktop server');
console.log('  2. Open the HTML client in a browser');
console.log(`  3. Connect to localhost:${WS_PORT} from the web client`);