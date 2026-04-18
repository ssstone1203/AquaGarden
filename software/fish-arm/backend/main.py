from fastapi import FastAPI, WebSocket, WebSocketDisconnect, HTTPException, Depends, status, BackgroundTasks
from fastapi.security import HTTPBearer, HTTPAuthorizationCredentials
from fastapi.middleware.cors import CORSMiddleware
from fastapi.staticfiles import StaticFiles
from fastapi.responses import StreamingResponse, FileResponse, JSONResponse
from fastapi.encoders import jsonable_encoder
from pydantic import BaseModel, Field, validator
from typing import Optional, List, Dict, Any
from sqlalchemy import create_engine, Column, Integer, String, DateTime
from sqlalchemy.ext.declarative import declarative_base
from sqlalchemy.orm import sessionmaker, Session
import asyncio
import json
import cv2
import numpy as np
from datetime import datetime, timedelta
from jose import JWTError, jwt
import bcrypt
import random
import logging
import time
from contextlib import asynccontextmanager
import os

# 配置日志
logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)

# 数据库配置
DATABASE_URL = "sqlite:///./aquagarden.db"

# 创建数据库引擎
engine = create_engine(DATABASE_URL, connect_args={"check_same_thread": False})
SessionLocal = sessionmaker(autocommit=False, autoflush=False, bind=engine)
Base = declarative_base()

# 数据库模型
class User(Base):
    __tablename__ = "users"
    
    id = Column(Integer, primary_key=True, index=True)
    username = Column(String(50), unique=True, index=True, nullable=False)
    email = Column(String(100), unique=True, index=True, nullable=True)
    hashed_password = Column(String(200), nullable=False)
    role = Column(String(20), default="user")
    created_at = Column(DateTime, default=datetime.utcnow)

# 创建表
Base.metadata.create_all(bind=engine)

def hash_password(password: str) -> str:
   return bcrypt.hashpw(password.encode('utf-8'), bcrypt.gensalt()).decode('utf-8')
 
# 创建默认管理员账号（如果不存在）
def create_default_admin():
    db = SessionLocal()
    try:
        existing_admin = db.query(User).filter(User.username == "admin").first()
        if not existing_admin:
            admin_user = User(
                username="admin",
                email="admin@aquagarden.com",
                hashed_password=hash_password("admin123"),
                role="admin"
            )
            db.add(admin_user)
            db.commit()
            logger.info("默认管理员账号已创建: admin / admin123")
    finally:
        db.close()

# 启动时创建默认管理员
create_default_admin()

# 获取数据库会话
def get_db():
    db = SessionLocal()
    try:
        yield db
    finally:
        db.close()

# 配置常量
class Config:
    SECRET_KEY = "your-secret-key-change-in-production"
    ALGORITHM = "HS256"
    ACCESS_TOKEN_EXPIRE_MINUTES = 30
    MAX_LOG_ENTRIES = 1000
    SENSOR_UPDATE_INTERVAL = 3  # 秒
    WEBSOCKET_PING_INTERVAL = 30  # 秒

# 密码加密
# 密码哈希函数（使用 bcrypt）

def verify_password(plain_password: str, hashed_password: str) -> bool:
    return bcrypt.checkpw(plain_password.encode('utf-8'), hashed_password.encode('utf-8'))
security = HTTPBearer()

app = FastAPI(title="AquaGarden API")

# CORS配置
app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)

# 静态文件：前端目录（相对 main.py 所在目录）
BASE_DIR = os.path.dirname(os.path.abspath(__file__))
FRONT_DIR = os.path.join(BASE_DIR, "..", "front")

# 挂载 /css 和 /js，使 login.html 等页面中的 css/login.css、js/login.js 能正确访问
app.mount("/css", StaticFiles(directory=os.path.join(FRONT_DIR, "css")), name="css")
app.mount("/js", StaticFiles(directory=os.path.join(FRONT_DIR, "js")), name="js")
app.mount("/static", StaticFiles(directory=FRONT_DIR), name="static")

# Pydantic模型
class RegisterRequest(BaseModel):
    username: str = Field(..., min_length=3, max_length=50, description="用户名")
    email: Optional[str] = Field(None, description="邮箱")
    password: str = Field(..., min_length=6, max_length=100, description="密码")

class LoginRequest(BaseModel):
    username: str = Field(..., min_length=3, max_length=50, description="用户名")
    password: str = Field(..., min_length=6, max_length=100, description="密码")

class Token(BaseModel):
    access_token: str
    token_type: str
    expires_in: int

class RobotControl(BaseModel):
    direction: str = Field(..., description="方向: up, down, left, right, forward, backward")
    
    @validator('direction')
    def validate_direction(cls, v):
        valid_directions = ['up', 'down', 'left', 'right', 'forward', 'backward']
        if v not in valid_directions:
            raise ValueError(f'方向必须是: {", ".join(valid_directions)}')
        return v

class ModeSwitch(BaseModel):
    mode: str = Field(..., description="模式: service, demo")
    
    @validator('mode')
    def validate_mode(cls, v):
        valid_modes = ['service', 'demo']
        if v not in valid_modes:
            raise ValueError(f'模式必须是: {", ".join(valid_modes)}')
        return v

class SensorData(BaseModel):
    temperature: float = Field(..., ge=0, le=100, description="温度 (°C)")
    ph: float = Field(..., ge=0, le=14, description="pH值")
    oxygen: float = Field(..., ge=0, le=20, description="溶解氧 (mg/L)")
    turbidity: float = Field(..., ge=0, le=100, description="浊度 (NTU)")

class LogMessage(BaseModel):
    timestamp: str
    type: str
    message: str

# 系统状态管理
class SystemState:
    def __init__(self):
        self._state = {
            "mode": "demo",
            "robot_position": {"x": 0, "y": 0, "z": 0},
            "sensors": {
                "temperature": 25.0,
                "ph": 7.0,
                "oxygen": 8.0,
                "turbidity": 10.0
            },
            "last_update": datetime.utcnow()
        }
        self._lock = asyncio.Lock()
    
    async def get_state(self, key: str = None):
        async with self._lock:
            if key:
                return self._state.get(key)
            return self._state.copy()
    
    async def update_state(self, key: str, value: Any):
        async with self._lock:
            self._state[key] = value
            self._state["last_update"] = datetime.utcnow()
    
    async def update_sensor_data(self, sensor_data: Dict[str, float]):
        async with self._lock:
            self._state["sensors"].update(sensor_data)
            self._state["last_update"] = datetime.utcnow()

system_state = SystemState()

# JWT相关函数
def create_access_token(data: dict, expires_delta: Optional[timedelta] = None):
    try:
        to_encode = data.copy()
        if expires_delta:
            expire = datetime.utcnow() + expires_delta
        else:
            expire = datetime.utcnow() + timedelta(minutes=Config.ACCESS_TOKEN_EXPIRE_MINUTES)
        to_encode.update({"exp": expire})
        encoded_jwt = jwt.encode(to_encode, Config.SECRET_KEY, algorithm=Config.ALGORITHM)
        return encoded_jwt
    except Exception as e:
        logger.error(f"创建JWT令牌失败: {e}")
        raise HTTPException(
            status_code=status.HTTP_500_INTERNAL_SERVER_ERROR,
            detail="无法创建认证令牌"
        )

async def verify_token(credentials: HTTPAuthorizationCredentials = Depends(security), db: Session = Depends(get_db)):
    try:
        token = credentials.credentials
        payload = jwt.decode(token, Config.SECRET_KEY, algorithms=[Config.ALGORITHM])
        username: str = payload.get("sub")
        if username is None:
            logger.warning(f"JWT令牌缺少用户信息: {payload}")
            raise HTTPException(
                status_code=status.HTTP_401_UNAUTHORIZED,
                detail="无效的认证凭据"
            )

        # 检查用户是否存在于数据库
        user = db.query(User).filter(User.username == username).first()
        if not user:
            logger.warning(f"JWT令牌中的用户不存在: {username}")
            raise HTTPException(
                status_code=status.HTTP_401_UNAUTHORIZED,
                detail="用户不存在"
            )

        return username
    except JWTError as e:
        logger.warning(f"JWT令牌验证失败: {e}")
        raise HTTPException(
            status_code=status.HTTP_401_UNAUTHORIZED,
            detail="无效的认证凭据"
        )
    except Exception as e:
        logger.error(f"令牌验证过程中发生错误: {e}")
        raise HTTPException(
            status_code=status.HTTP_500_INTERNAL_SERVER_ERROR,
            detail="认证过程发生错误"
        )

# API路由
@app.get("/")
async def root():
    """根路径 - 重定向到登录页面"""
    from fastapi.responses import RedirectResponse
    return RedirectResponse(url="/login.html")

@app.get("/login")
async def login_page():
    """登录页面"""
    return FileResponse(os.path.join(FRONT_DIR, "login.html"))

@app.get("/login.html")
async def login_html_page():
    """登录页面（.html后缀）"""
    return FileResponse(os.path.join(FRONT_DIR, "login.html"))

@app.get("/register")
async def register_page():
    """注册页面"""
    return FileResponse(os.path.join(FRONT_DIR, "register.html"))

@app.get("/register.html")
async def register_html_page():
    """注册页面（.html后缀）"""
    return FileResponse(os.path.join(FRONT_DIR, "register.html"))

@app.get("/index")
async def index_page():
    """主页/仪表板"""
    return FileResponse(os.path.join(FRONT_DIR, "index.html"))

@app.get("/index.html")
async def index_html_page():
    """主页/仪表板（.html后缀）"""
    return FileResponse(os.path.join(FRONT_DIR, "index.html"))

@app.get("/cameras")
async def cameras_page():
    """视频监控页面"""
    return FileResponse(os.path.join(FRONT_DIR, "cameras.html"))

@app.get("/cameras.html")
async def cameras_html_page():
    """视频监控页面（.html后缀）"""
    return FileResponse(os.path.join(FRONT_DIR, "cameras.html"))

@app.get("/history")
async def history_page():
    """历史数据页面"""
    return FileResponse(os.path.join(FRONT_DIR, "history.html"))

@app.get("/history.html")
async def history_html_page():
    """历史数据页面（.html后缀）"""
    return FileResponse(os.path.join(FRONT_DIR, "history.html"))

@app.get("/robot")
async def robot_page():
    """机械臂控制页面"""
    return FileResponse(os.path.join(FRONT_DIR, "robot.html"))

@app.get("/robot.html")
async def robot_html_page():
    """机械臂控制页面（.html后缀）"""
    return FileResponse(os.path.join(FRONT_DIR, "robot.html"))

@app.get("/alerts")
async def alerts_page():
    """警报设置页面"""
    return FileResponse(os.path.join(FRONT_DIR, "alerts.html"))

@app.get("/alerts.html")
async def alerts_html_page():
    """警报设置页面（.html后缀）"""
    return FileResponse(os.path.join(FRONT_DIR, "alerts.html"))

@app.get("/settings")
async def settings_page():
    """系统设置页面"""
    return FileResponse(os.path.join(FRONT_DIR, "settings.html"))

@app.get("/settings.html")
async def settings_html_page():
    """系统设置页面（.html后缀）"""
    return FileResponse(os.path.join(FRONT_DIR, "settings.html"))

@app.post("/api/register")
async def register(register_data: RegisterRequest, db: Session = Depends(get_db)):
    """用户注册API"""
    # 检查用户名是否已存在
    existing_user = db.query(User).filter(User.username == register_data.username).first()
    if existing_user:
        raise HTTPException(
            status_code=status.HTTP_400_BAD_REQUEST,
            detail="用户名已存在"
        )
    
    # 如果提供了邮箱，检查邮箱是否已存在
    if register_data.email:
        existing_email = db.query(User).filter(User.email == register_data.email).first()
        if existing_email:
            raise HTTPException(
                status_code=status.HTTP_400_BAD_REQUEST,
                detail="邮箱已被注册"
            )
    
    # 加密密码
    hashed_password = hash_password(register_data.password)
    
    # 创建新用户
    new_user = User(
        username=register_data.username,
        email=register_data.email,
        hashed_password=hashed_password,
        role="user"
    )
    
    db.add(new_user)
    db.commit()
    db.refresh(new_user)
    
    logger.info(f"新用户注册: {register_data.username}")
    
    return {
        "success": True,
        "message": "注册成功",
        "user": {
            "username": new_user.username,
            "email": new_user.email,
            "role": new_user.role
        }
    }

@app.post("/api/login", response_model=Token)
async def login(login_data: LoginRequest, db: Session = Depends(get_db)):
    """用户登录API"""
    # 从数据库查询用户
    user = db.query(User).filter(User.username == login_data.username).first()
    
    if not user or not verify_password(login_data.password, user.hashed_password):
        raise HTTPException(
            status_code=status.HTTP_401_UNAUTHORIZED,
            detail="用户名或密码错误",
        )
    
    access_token_expires = timedelta(minutes=Config.ACCESS_TOKEN_EXPIRE_MINUTES)
    access_token = create_access_token(
        data={"sub": user.username}, expires_delta=access_token_expires
    )
    return {"access_token": access_token, "token_type": "bearer", "expires_in": Config.ACCESS_TOKEN_EXPIRE_MINUTES * 60}

@app.get("/api/users")
async def get_users(db: Session = Depends(get_db)):
    """获取所有用户（仅管理员）"""
    users = db.query(User).all()
    return [{"id": u.id, "username": u.username, "email": u.email, "role": u.role, "created_at": u.created_at.isoformat() if u.created_at else None} for u in users]

@app.get("/api/sensors")
async def get_sensors(username: str = Depends(verify_token)):
    # 模拟传感器数据波动
    system_state["sensors"]["temperature"] = round(25.0 + random.uniform(-0.5, 0.5), 2)
    system_state["sensors"]["ph"] = round(7.0 + random.uniform(-0.2, 0.2), 2)
    system_state["sensors"]["oxygen"] = round(8.0 + random.uniform(-0.3, 0.3), 2)
    system_state["sensors"]["turbidity"] = round(10.0 + random.uniform(-1.0, 1.0), 2)
    return system_state["sensors"]

@app.post("/api/robot/control")
async def control_robot(control: RobotControl, username: str = Depends(verify_token)):
    # 模拟机械臂控制
    direction_map = {
        "up": (0, 0, 1),
        "down": (0, 0, -1),
        "left": (-1, 0, 0),
        "right": (1, 0, 0),
        "forward": (0, 1, 0),
        "backward": (0, -1, 0)
    }
    
    if control.direction in direction_map:
        dx, dy, dz = direction_map[control.direction]
        system_state["robot_position"]["x"] += dx
        system_state["robot_position"]["y"] += dy
        system_state["robot_position"]["z"] += dz
        
        # 广播日志消息
        log_message = {
            "timestamp": datetime.now().isoformat(),
            "type": "robot",
            "message": f"机械臂移动: {control.direction}, 位置: {system_state['robot_position']}"
        }
        await manager.broadcast(json.dumps(log_message))
        
        return {"status": "success", "position": system_state["robot_position"]}
    
    raise HTTPException(status_code=400, detail="Invalid direction")

@app.post("/api/mode")
async def switch_mode(mode_data: ModeSwitch, username: str = Depends(verify_token)):
    if mode_data.mode not in ["service", "demo"]:
        raise HTTPException(status_code=400, detail="Invalid mode")
    
    system_state["mode"] = mode_data.mode
    
    # 广播日志消息
    log_message = {
        "timestamp": datetime.now().isoformat(),
        "type": "system",
        "message": f"模式切换: {mode_data.mode}"
    }
    await manager.broadcast(json.dumps(log_message))
    
    return {"status": "success", "mode": system_state["mode"]}

@app.get("/api/mode")
async def get_mode(username: str = Depends(verify_token)):
    return {"mode": system_state["mode"]}

# WebSocket连接管理改进
class ConnectionManager:
    def __init__(self):
        self.active_connections: List[WebSocket] = []
        self._lock = asyncio.Lock()

    async def connect(self, websocket: WebSocket):
        await websocket.accept()
        async with self._lock:
            self.active_connections.append(websocket)
        logger.info(f"WebSocket连接建立，当前连接数: {len(self.active_connections)}")

    async def disconnect(self, websocket: WebSocket):
        async with self._lock:
            if websocket in self.active_connections:
                self.active_connections.remove(websocket)
        logger.info(f"WebSocket连接断开，当前连接数: {len(self.active_connections)}")

    async def broadcast(self, message: str):
        disconnected = []
        async with self._lock:
            for connection in self.active_connections:
                try:
                    await connection.send_text(message)
                except Exception as e:
                    logger.warning(f"发送WebSocket消息失败: {e}")
                    disconnected.append(connection)
            
            # 移除断开的连接
            for connection in disconnected:
                self.active_connections.remove(connection)

manager = ConnectionManager()

# WebSocket日志推送
@app.websocket("/ws/logs")
async def websocket_logs(websocket: WebSocket):
    """WebSocket实时日志推送"""
    await manager.connect(websocket)
    try:
        # 发送连接成功消息
        initial_message = {
            "timestamp": datetime.now().isoformat(),
            "type": "system",
            "message": "WebSocket连接已建立"
        }
        await websocket.send_text(json.dumps(initial_message))
        
        # 定期发送心跳和系统状态
        last_ping = time.time()
        while True:
            # 发送心跳
            current_time = time.time()
            if current_time - last_ping > Config.WEBSOCKET_PING_INTERVAL:
                try:
                    await websocket.send_text(json.dumps({
                        "timestamp": datetime.now().isoformat(),
                        "type": "heartbeat",
                        "message": "ping"
                    }))
                    last_ping = current_time
                except Exception as e:
                    logger.warning(f"发送心跳失败: {e}")
                    break
            
            # 短暂休眠避免CPU占用过高
            await asyncio.sleep(1)
            
    except WebSocketDisconnect:
        logger.info("WebSocket连接正常断开")
    except Exception as e:
        logger.error(f"WebSocket连接发生错误: {e}")
    finally:
        await manager.disconnect(websocket)

# 视频流生成器（模拟）
def generate_fake_video():
    """生成模拟视频帧"""
    while True:
        # 创建一个随机噪声图像作为模拟视频
        img = np.random.randint(0, 255, (480, 640, 3), dtype=np.uint8)
        
        # 添加文字
        cv2.putText(img, f"Time: {datetime.now().strftime('%H:%M:%S')}", 
                    (10, 30), cv2.FONT_HERSHEY_SIMPLEX, 0.7, (255, 255, 255), 2)
        cv2.putText(img, "Camera Feed", 
                    (10, 60), cv2.FONT_HERSHEY_SIMPLEX, 0.7, (0, 255, 0), 2)
        
        # 编码为JPEG
        ret, buffer = cv2.imencode('.jpg', img)
        frame = buffer.tobytes()
        
        yield (b'--frame\r\n'
               b'Content-Type: image/jpeg\r\n\r\n' + frame + b'\r\n')
        
        asyncio.get_event_loop().run_until_complete(asyncio.sleep(0.1))

@app.get("/api/video/robot")
async def video_robot(username: str = Depends(verify_token)):
    """机械臂摄像头视频流"""
    return StreamingResponse(generate_fake_video(), 
                           media_type="multipart/x-mixed-replace; boundary=frame")

@app.get("/api/video/tank")
async def video_tank(username: str = Depends(verify_token)):
    """鱼缸摄像头视频流"""
    return StreamingResponse(generate_fake_video(), 
                           media_type="multipart/x-mixed-replace; boundary=frame")

if __name__ == "__main__":
    import uvicorn
    uvicorn.run(app, host="localhost", port=8090)
