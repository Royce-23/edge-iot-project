from sqlalchemy import create_engine
from sqlalchemy.orm import sessionmaker, declarative_base

# Tạo file cơ sở dữ liệu có tên là iot_data.db nằm ngay trong thư mục backend
SQLALCHEMY_DATABASE_URL = "sqlite:///./iot_data.db"

# Kết nối SQLite (check_same_thread=False là bắt buộc dùng cho FastAPI)
engine = create_engine(
    SQLALCHEMY_DATABASE_URL, connect_args={"check_same_thread": False}
)
SessionLocal = sessionmaker(autocommit=False, autoflush=False, bind=engine)

Base = declarative_base()

# Hàm này giống như "người giữ cửa" cấp phát kết nối Database cho API
def get_db():
    db = SessionLocal()
    try:
        yield db
    finally:
        db.close()