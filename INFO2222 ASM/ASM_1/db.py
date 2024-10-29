'''
db
database file, containing all the logic to interface with the sql database
'''

from sqlalchemy import create_engine
from sqlalchemy.orm import sessionmaker
from models import *
import bcrypt
from pathlib import Path

# creates the database directory
Path("database").mkdir(exist_ok=True)

# "database/main.db" specifies the database file
# change it if you wish
# turn echo = True to display the sql output
engine = create_engine("sqlite:///database/main.db", echo=False)
Session = sessionmaker(bind=engine)
# initializes the database
Base.metadata.create_all(engine)
# session = Session()
# inserts a user to the database
# Updating the Insert User function
def insert_user(username: str, password: str, public_key: str):
    salt = bcrypt.gensalt()
    hashed_password = bcrypt.hashpw(password.encode('utf-8'), salt)
    with Session() as session:
        user = User(username=username, password=hashed_password, public_key=public_key)
        session.add(user)
        session.commit()

# gets a user from the database
def get_user(username: str):
    # Use Session() to create a new session without passing engine
    with Session() as session:
        return session.get(User, username)


# Initialise rooms based on existing users
def initialize_rooms(session):
    users = session.query(User).all()
    existing_pairs = set()

    for user1 in users:
        for user2 in users:
            if user1.username != user2.username and (user1.username, user2.username) not in existing_pairs and (
            user2.username, user1.username) not in existing_pairs:
                existing_pairs.add((user1.username, user2.username))
                room = RoomTable(user1_id=user1.username, user2_id=user2.username)
                session.add(room)
    session.commit()


# Update rooms when new users are added
def update_rooms_with_new_user(session, new_user_username):
    # Get all existing users except new users
    existing_users = session.query(User.username).filter(User.username != new_user_username).all()

    for user in existing_users:
        # Check if the room already exists
        existing_room = session.query(RoomTable).filter(
            ((RoomTable.user1_id == new_user_username) & (RoomTable.user2_id == user.username)) |
            ((RoomTable.user1_id == user.username) & (RoomTable.user2_id == new_user_username))
        ).first()

        if not existing_room:
            # If it doesn't exist, create a new room
            room = RoomTable(user1_id=new_user_username, user2_id=user.username)
            session.add(room)

    try:
        session.commit()
    except Exception as e:
        session.rollback()  # Rollback on Error
        print(f"Error updating rooms for new user {new_user_username}: {e}")


