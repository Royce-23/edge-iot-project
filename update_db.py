import sqlite3

DB = "iot_data.db"

conn = sqlite3.connect(DB)
cursor = conn.cursor()

columns = [row[1] for row in cursor.execute("PRAGMA table_info(health_events)")]

if "acknowledged" not in columns:
    cursor.execute("""
        ALTER TABLE health_events
        ADD COLUMN acknowledged BOOLEAN NOT NULL DEFAULT 0
    """)
    print("✓ Đã thêm cột acknowledged")
else:
    print("✓ acknowledged đã tồn tại")

if "acknowledged_at" not in columns:
    cursor.execute("""
        ALTER TABLE health_events
        ADD COLUMN acknowledged_at VARCHAR
    """)
    print("✓ Đã thêm cột acknowledged_at")
else:
    print("✓ acknowledged_at đã tồn tại")

conn.commit()

print("\nCấu trúc hiện tại:")
for row in cursor.execute("PRAGMA table_info(health_events)"):
    print(row)

conn.close()