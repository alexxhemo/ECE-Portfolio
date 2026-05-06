from app import create_app
from app.db import init_db

app = create_app()

if __name__ == "__main__":
    with app.app_context():
        init_db()
        print("Database initialized.")