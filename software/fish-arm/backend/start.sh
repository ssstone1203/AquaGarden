#!/bin/bash

echo "Starting AquaGarden Server..."
echo ""

cd "$(dirname "$0")"

if [ ! -d "venv" ]; then
    echo "Creating virtual environment..."
    python3 -m venv venv
fi

echo "Activating virtual environment..."
source venv/bin/activate

echo "Installing dependencies..."
pip install -r requirements.txt

echo ""
echo "Starting server at http://localhost:8000"
echo ""
echo "Default login credentials:"
echo "Username: admin"
echo "Password: admin123"
echo ""

python main.py

