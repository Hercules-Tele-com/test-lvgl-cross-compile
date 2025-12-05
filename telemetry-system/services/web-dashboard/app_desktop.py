#!/usr/bin/env python3
"""
Desktop Dashboard Server
Lightweight Flask server on port 80 serving the original dashboard for remote viewing
"""

import os
import sys
import logging
from flask import Flask, send_from_directory
from flask_cors import CORS

# Setup logging
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s [%(levelname)s] %(message)s'
)
logger = logging.getLogger(__name__)

# Flask app
app = Flask(__name__, static_folder='static', static_url_path='')
CORS(app)

@app.route('/')
def index():
    """Serve original desktop dashboard"""
    return send_from_directory('static', 'index.html')

@app.route('/api/redirect')
def api_redirect():
    """Redirect API requests to main dashboard on port 8080"""
    return {
        "message": "API available on port 8080",
        "api_url": "http://localhost:8080/api/status"
    }, 200

def main():
    """Main entry point"""
    logger.info("Starting desktop dashboard server on port 80")
    logger.info("Original dashboard available at: http://localhost:80")
    logger.info("API available at: http://localhost:8080/api/status")
    logger.info("Kiosk mode available at: http://localhost:8080")

    try:
        app.run(
            host='0.0.0.0',
            port=80,
            debug=False
        )
    except PermissionError:
        logger.error("Permission denied: Port 80 requires root/sudo privileges")
        logger.info("Try running with: sudo python3 app_desktop.py")
        return 1
    except Exception as e:
        logger.error(f"Failed to start server: {e}")
        return 1

    return 0

if __name__ == "__main__":
    sys.exit(main())
