#!/bin/bash
# Diagnose FT5316 touchscreen issues

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

echo -e "${BLUE}================================================================${NC}"
echo -e "${BLUE}   FT5316 Touchscreen Diagnostics${NC}"
echo -e "${BLUE}================================================================${NC}"
echo ""

echo -e "${YELLOW}1. Checking service file...${NC}"
if [ -f /etc/systemd/system/ft5316-touchscreen.service ]; then
    echo -e "  ${GREEN}✓${NC} Service file exists"
    echo "  Contents:"
    cat /etc/systemd/system/ft5316-touchscreen.service
else
    echo -e "  ${RED}✗${NC} Service file not found"
fi
echo ""

echo -e "${YELLOW}2. Checking for touchscreen script...${NC}"
if [ -f /home/pi/ft5316_touch.py ]; then
    echo -e "  ${GREEN}✓${NC} Found at /home/pi/ft5316_touch.py"
elif [ -f /home/emboo/ft5316_touch.py ]; then
    echo -e "  ${GREEN}✓${NC} Found at /home/emboo/ft5316_touch.py"
else
    echo -e "  ${RED}✗${NC} Touchscreen script not found in /home/pi or /home/emboo"
    echo "  Searching..."
    find /home -name "ft5316*.py" 2>/dev/null || echo "  No FT5316 scripts found"
fi
echo ""

echo -e "${YELLOW}3. Checking if user 'pi' exists...${NC}"
if id pi &>/dev/null; then
    echo -e "  ${GREEN}✓${NC} User 'pi' exists"
else
    echo -e "  ${RED}✗${NC} User 'pi' does NOT exist (this is why service fails with exit code 217)"
fi
echo ""

echo -e "${YELLOW}4. Checking current user...${NC}"
echo "  Current user: $USER"
echo "  Home directory: $HOME"
echo ""

echo -e "${YELLOW}5. Checking I2C devices...${NC}"
if command -v i2cdetect &> /dev/null; then
    echo "  I2C devices on bus 1:"
    sudo i2cdetect -y 1 2>/dev/null || echo "  Failed to detect I2C devices"
else
    echo -e "  ${YELLOW}⚠${NC} i2c-tools not installed"
fi
echo ""

echo -e "${YELLOW}6. Checking if I2C is enabled...${NC}"
if [ -e /dev/i2c-1 ]; then
    echo -e "  ${GREEN}✓${NC} /dev/i2c-1 exists"
    ls -l /dev/i2c-1
else
    echo -e "  ${RED}✗${NC} /dev/i2c-1 not found - I2C may not be enabled"
    echo "  Enable I2C with: sudo raspi-config"
fi
echo ""

echo -e "${YELLOW}7. Checking Python dependencies...${NC}"
python3 -c "import evdev" 2>/dev/null && echo -e "  ${GREEN}✓${NC} python3-evdev installed" || echo -e "  ${RED}✗${NC} python3-evdev not installed"
python3 -c "import smbus2" 2>/dev/null && echo -e "  ${GREEN}✓${NC} python3-smbus2 installed" || echo -e "  ${RED}✗${NC} python3-smbus2 not installed"
echo ""

echo -e "${BLUE}================================================================${NC}"
echo -e "${BLUE}   Diagnosis Complete${NC}"
echo -e "${BLUE}================================================================${NC}"
echo ""
echo "Common issues:"
echo "  1. Service file references user 'pi' but current user is '$USER'"
echo "  2. Touchscreen script doesn't exist at /home/pi/ft5316_touch.py"
echo "  3. Need to install driver from GitHub repo"
echo ""
