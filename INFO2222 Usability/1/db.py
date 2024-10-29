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

def insert_admin(username: str, password: str, public_key: str, role: str):
    salt = bcrypt.gensalt()
    hashed_password = bcrypt.hashpw(password.encode('utf-8'), salt)
    with Session() as session:
        user = User(username=username, password=hashed_password, public_key=public_key, role=role)
        session.add(user)
        session.commit()

def insert_post(title: str, content: str, username: str):
    with Session() as session:
        user = session.query(User).filter_by(username=username).first()
        if user:
            new_post = Post(title=title, content=content, user_id=user.username)
            session.add(new_post)
            session.commit()
            return True
        return False

def insert_comment(post_id: int, content: str, username: str):
    with Session() as session:
        new_comment = Comment(content=content, post_id=post_id, user_id=username)
        session.add(new_comment)
        session.commit()
        return True

def get_posts():
    with Session() as session:
        # Getting post and author information using a join query
        posts = session.query(Post).join(User, User.username == Post.user_id).add_columns(
            Post.id, Post.title, Post.content, User.username.label("author")
        ).all()
        return [{
            'id': post.id,
            'title': post.title,
            'content': post.content,
            'author': post.author
        } for post in posts]

def get_post(post_id):
    with Session() as session:
        # Get specific post and author information using a join query
        post = session.query(Post).join(User, User.username == Post.user_id).add_columns(
            Post.id, Post.title, Post.content, User.username.label("author")
        ).filter(Post.id == post_id).one_or_none()  # Use one_or_none to ensure that if the post is not found it returns None

        if post:
            return {
                'id': post.id,
                'title': post.title,
                'content': post.content,
                'author': post.author
            }
        else:
            return None

def get_comments(post_id: int):
    with Session() as session:
        comments = session.query(Comment).filter_by(post_id=post_id).join(User, User.username == Comment.user_id).add_columns(
            Comment.id,  # Make sure you get the ID of the comment here
            Comment.content,
            User.username.label('author')
        ).all()
        return [{
            'id': comment.id,  # Add the id to the returned data here
            'content': comment.content,
            'author': comment.author
        } for comment in comments]

def get_comment(comment_id):
    with Session() as session:
        # Connection query to get reviews and their authors
        comment = session.query(Comment).filter_by(id=comment_id).join(User, User.username == Comment.user_id).add_columns(
            Comment.id, Comment.content, User.username.label('author')
        ).one_or_none()
        if comment:
            return {
                'id': comment.id,
                'content': comment.content,
                'author': comment.author
            }
        else:
            return None

# gets a user from the database
def get_user(username: str):
    # Use Session() to create a new session without passing engine
    with Session() as session:
        return session.get(User, username)

def get_user_role(username: str):
    with Session() as session:
        user = session.query(User).filter_by(username=username).first()
        return user.role if user else None

def get_all_users():
    with Session() as session:
        return session.query(User).all()

def update_user_role(username, new_role):
    with Session() as session:
        user = session.query(User).filter_by(username=username).one_or_none()
        if user:
            user.role = new_role
            session.commit()
            return True
    return False

def delete_post(post_id):
    with Session() as session:
        post = session.query(Post).get(post_id)
        if post:
            session.delete(post)
            session.commit()
            return True
    return False

def update_post(post_id, title, content):
    with Session() as session:
        post = session.query(Post).filter_by(id=post_id).one_or_none()
        if post:
            post.title = title
            post.content = content
            session.commit()
            return True
    return False

def delete_comment(comment_id):
    with Session() as session:
        comment = session.query(Comment).get(comment_id)
        if comment:
            session.delete(comment)
            session.commit()
            return True
    return False







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


