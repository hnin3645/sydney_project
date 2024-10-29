from flask_socketio import join_room, emit, leave_room
from flask import request
from sqlalchemy.orm import sessionmaker
from sqlalchemy import create_engine
from models import Room, RoomTable, User, Message
# from models import Room, RoomTable, User

import db
import hmac
import hashlib

SECRET_KEY = 'c2FkU0Fk'

# Connecting to the database
engine = create_engine("sqlite:///database/main.db", echo=True)
Session = sessionmaker(bind=engine)

try:
    from __main__ import socketio
except ImportError:
    from app import socketio

# Create a Room instance, using the database session
session = Session()
room_service = Room(session)

# Client connects to socket
@socketio.on('connect')
def connect():
    username = request.cookies.get("username")
    room_id = request.cookies.get("room_id")
    if room_id is None or username is None:
        print("Connection failed: username or room_id missing.")
        return

    try:
        room_id_int = int(room_id)  # Ensure that room_id is correctly converted to an integer.
    except ValueError:
        print(f"Error converting room_id to integer: {room_id}")
        return

    print(f"User {username} attempting to connect to room {room_id_int}")

    join_room(room_id_int)  # Join the room
    print(f"User {username} joined room {room_id_int}")

    emit("system_message", {
        "content": f"{username} has connected",
        "color": "green"
    }, to=room_id_int)

    # Query history messages
    messages = session.query(Message).filter_by(room_id=room_id_int).order_by(Message.message_time).all()
    if messages:
        print(f"Sending {len(messages)} historical messages to {username} in room {room_id_int}")
    else:
        print(f"No historical messages found for room {room_id_int}")

    # Send a history message to the user who just connected
    for message in messages:
        emit("user_message", {
            "username": message.sender_username,
            "encryptedMessageSender": message.content_sender,
            "encryptedMessageReceiver": message.content_receiver
        }, to=request.sid)

    print(f"Completed connection setup for {username} in room {room_id_int}")



# Client disconnects
@socketio.on('disconnect')
def disconnect():
    username = request.cookies.get("username")
    room_id = request.cookies.get("room_id")
    if room_id is None or username is None:
        return
    # Adjusting the system message format
    emit("system_message", {
        "content": f"{username} has disconnected",
        "color": "red"
    }, to=int(room_id))
    print("Disconnected", room_id)


# Send Message Event Handling
@socketio.on("send")
def send(username, encrypted_message_sender, encrypted_message_receiver, hmac, room_id):
    # Verify HMAC
    expected_hmac = create_hmac(encrypted_message_sender, SECRET_KEY)
    if not verify_hmac(hmac, expected_hmac):
        print("HMAC verification failed.")
        return

    # Storing Messages
    new_message = Message(sender_username=username, room_id=room_id, content_sender=encrypted_message_sender, content_receiver=encrypted_message_receiver)
    session.add(new_message)
    session.commit()

    # Send a message to all clients in the room, including the username of the message sender, and two versions of the encrypted message
    emit("user_message", {
        "username": username,
        "encryptedMessageSender": encrypted_message_sender,
        "encryptedMessageReceiver": encrypted_message_receiver
    }, to=room_id)


# Add room event handling
@socketio.on("join")
def join(sender_name, receiver_name):
    sender = db.get_user(sender_name)
    if sender is None:
        return "Unknown sender!"

    receiver = db.get_user(receiver_name)
    if receiver is None:
        return "Unknown receiver!"

    room_id = room_service.get_room(sender_name, receiver_name)

    room_service.join_room(sender_name, room_id)
    join_room(room_id)

    emit("system_message", {
        "content": f"{sender_name} has joined the room. Now talking to {receiver_name}.",
        "color": "green"
    }, room=room_id)
    print("Joined the room.", room_id)

    # Send history messages only to joined users (not the whole room)
    messages = session.query(Message).filter_by(room_id=room_id).order_by(Message.message_time).all()
    for message in messages:
        emit("user_message", {
            "username": message.sender_username,
            "encryptedMessageSender": message.content_sender,
            "encryptedMessageReceiver": message.content_receiver
        }, to=request.sid)

    return room_id



# Leaving the room event handling
@socketio.on("leave")
def leave(username, room_id):
    # Adjusting the system message format
    emit("system_message", {
        "content": f"{username} has left the room.",
        "color": "red"
    }, to=room_id)
    leave_room(room_id)
    room_service.leave_room(username)
    print(f"{username} has left the room, Room ID: {room_id}")

def create_hmac(message, key):
    return hmac.new(key.encode(), message.encode(), hashlib.sha256).hexdigest()

def verify_hmac(received_hmac, expected_hmac):
    return hmac.compare_digest(received_hmac, expected_hmac)