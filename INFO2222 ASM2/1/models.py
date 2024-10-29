'''
models
defines sql alchemy data models
also contains the definition for the room class used to keep track of socket.io rooms

Just a sidenote, using SQLAlchemy is a pain. If you want to go above and beyond, 
do this whole project in Node.js + Express and use Prisma instead, 
Prisma docs also looks so much better in comparison

or use SQLite, if you're not into fancy ORMs (but be mindful of Injection attacks :) )
'''

from sqlalchemy import create_engine, Column, Integer, String, ForeignKey, Table, Enum, UniqueConstraint
from sqlalchemy.ext.declarative import declarative_base
from sqlalchemy.orm import relationship, Mapped, mapped_column
from typing import Dict,List
from sqlalchemy import DateTime
from sqlalchemy.sql import func
# data models
# class Base(DeclarativeBase):
#     pass
Base = declarative_base()
# model to store user information
association_table = Table('friend_association', Base.metadata,
                          Column('user_id', String, ForeignKey('user.username')),
                          Column('friend_id', String, ForeignKey('user.username'))
                          )


class User(Base):
    __tablename__ = 'user'

    username: Mapped[str] = mapped_column(String, primary_key=True)
    password: Mapped[str] = mapped_column(String)
    public_key: Mapped[str] = mapped_column(String)  # Add public key column
    rooms = relationship("RoomTable", primaryjoin="or_(User.username==RoomTable.user1_id, User.username==RoomTable.user2_id)")

    # Define buddy relationships between users
    friends = relationship(
        "User",
        secondary=association_table,
        primaryjoin=username == association_table.c.user_id,
        secondaryjoin=username == association_table.c.friend_id,
        backref="added_friends"
    )


class FriendRequest(Base):
    __tablename__ = 'friend_requests'
    id = Column(Integer, primary_key=True)
    sender_username = Column(String, ForeignKey('user.username'))
    receiver_username = Column(String, ForeignKey('user.username'))
    status = Column(Enum('pending', 'accepted', 'rejected', name='request_status'))

    sender = relationship("User", foreign_keys=[sender_username])
    receiver = relationship("User", foreign_keys=[receiver_username])


# Room class, used to keep track of which username is in which room
class RoomTable(Base):
    __tablename__ = 'rooms'

    id = Column(Integer, primary_key=True)
    user1_id = Column(String, ForeignKey('user.username'))
    user2_id = Column(String, ForeignKey('user.username'))

    user1 = relationship("User", foreign_keys=[user1_id], overlaps="rooms")
    user2 = relationship("User", foreign_keys=[user2_id], overlaps="rooms")

    # Ensure that each user pair is unique
    __table_args__ = (UniqueConstraint('user1_id', 'user2_id', name='_user_pair_uc'),)

class Message(Base):
    __tablename__ = 'messages'

    id = Column(Integer, primary_key=True)
    sender_username = Column(String, ForeignKey('user.username'), nullable=False)
    room_id = Column(Integer, ForeignKey('rooms.id'), nullable=False)
    content_receiver = Column(String, nullable=False)  # Message content encrypted by the receiver's public key
    content_sender = Column(String, nullable=False)    # Message content encrypted with the sender's public key
    message_time = Column(DateTime, default=func.now(), nullable=False)

    sender = relationship("User", foreign_keys=[sender_username])
    room = relationship("RoomTable", foreign_keys=[room_id])


class Room:
    def __init__(self, session):
        self.session = session
        self.dict = {}  # Cache user room IDs

    def get_room(self, sender: str, receiver: str) -> int:
        try:
            # Check if the room already exists in the database
            existing_room = self.session.query(RoomTable).filter(
                ((RoomTable.user1_id == sender) & (RoomTable.user2_id == receiver)) |
                ((RoomTable.user1_id == receiver) & (RoomTable.user2_id == sender))
            ).first()

            if existing_room is None:
                # If it doesn't exist, create a new room
                new_room = RoomTable(user1_id=sender, user2_id=receiver)
                self.session.add(new_room)
                self.session.commit()
                room_id = new_room.id
            else:
                room_id = existing_room.id

            # Updating the in-memory mapping
            self.dict[sender] = room_id
            self.dict[receiver] = room_id

            return room_id
        except Exception as e:
            print(f"Error accessing or creating room: {e}")
            self.session.rollback()
            return None  # Or you can throw an exception

    def leave_room(self, user: str):
        # Remove the user's room ID from memory
        if user in self.dict:
            room_id = self.dict.pop(user)

            return room_id

    def join_room(self, user: str, room_id: int):
        # Join the room and update the mapping in memory
        self.dict[user] = room_id
