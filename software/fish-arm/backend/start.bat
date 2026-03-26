@echo off
echo Starting AquaGarden Server...
echo.

cd /d %~dp0

if not exist venv (
    echo Creating virtual environment...
    python -m venv venv
)

echo Activating virtual environment...
call venv\Scripts\activate.bat

echo Installing dependencies...
pip install -r requirements.txt

echo.
echo Starting server at http://localhost:8000
echo.
echo Default login credentials:
echo Username: admin
echo Password: admin123
echo.

python main.py

pause

