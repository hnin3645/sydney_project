'''
app.py contains all of the server application
this is where you'll find all of the get/post request handlers
the socket event handlers are inside of socket_routes.py
'''

from flask import Flask, render_template, request, abort, url_for, session, redirect, request, jsonify
from flask_socketio import SocketIO
import db
import secrets
import bcrypt
# import logging
from cryptography.hazmat.primitives.asymmetric import rsa
from cryptography.hazmat.primitives import serialization
from werkzeug.utils import safe_join
from flask import current_app
import os

from models import User, FriendRequest
# this turns off Flask Logging, uncomment this to turn off Logging
# log = logging.getLogger('werkzeug')
# log.setLevel(logging.ERROR)
from db import Session as DBSession
app = Flask(__name__)

# secret key used to sign the session cookie
app.config['SECRET_KEY'] = secrets.token_hex()
socketio = SocketIO(app)

# don't remove this!!
import socket_routes


# index page
@app.route("/")
def index():
    return render_template("index.jinja")

@app.route('/ed')
def ed_page():
    if 'username' in session:
        username = session['username']
        role = db.get_user_role(username)  # Get user roles
        posts = db.get_posts()  # Getting data using the modified get_posts function
        return render_template('ed.jinja', username=username, role=role, posts=posts)
    else:
        return redirect(url_for('index'))

@app.route('/manage_roles')
def manage_roles():
    if 'username' in session:
        role = db.get_user_role(session['username'])
        if role in ['admin', 'staff']:
            users = db.get_all_users()
            return render_template('manage_roles.jinja', users=users, available_roles=['admin', 'staff', 'student', 'guest'])
        else:
            return redirect(url_for('index'))
    return redirect(url_for('login'))

@app.route('/update_user_role', methods=['POST'])
def update_user_role():
    if 'username' in session and request.is_json:
        data = request.get_json()
        username = data.get('username')
        new_role = data.get('role')
        current_role = db.get_user_role(session['username'])

        if (current_role == 'admin') or (current_role == 'staff' and new_role in ['student', 'guest']):
            if db.update_user_role(username, new_role):
                return jsonify({'message': 'Role updated successfully'}), 200
            else:
                return jsonify({'error': 'Failed to update role'}), 500
        else:
            return jsonify({'error': 'Permission denied'}), 403
    return jsonify({'error': 'Not authorized'}), 401

@app.route('/create_topic', methods=['POST'])
def create_topic():
    if request.method == 'POST' and 'username' in session:
        title = request.form['title']
        content = request.form['content']
        username = session['username']
        if db.insert_post(title, content, username):
            return redirect(url_for('ed_page'))
        else:
            return "Error: Unable to save post", 400
    return redirect(url_for('login'))

@app.route('/get_comments')
def get_comments():
    post_id = request.args.get('post_id')
    if not post_id:
        return jsonify({'error': 'No post_id provided'}), 400

    comments = db.get_comments(int(post_id))
    return jsonify(comments)

@app.route('/submit_comment', methods=['POST'])
def submit_comment():
    if not 'username' in session:
        return jsonify({'error': 'User not logged in'}), 401
    if session.get('role') == 'guest':
        return jsonify({'error': 'Not authorized to post comments'}), 403

    post_id = request.form.get('post_id')
    comment_content = request.form.get('comment')
    username = session['username']

    if not post_id or not comment_content:
        return jsonify({'error': 'Missing data'}), 400

    success = db.insert_comment(post_id, comment_content, username)
    if success:
        return jsonify({'message': 'Comment added successfully'}), 200
    else:
        return jsonify({'error': 'Failed to add comment'}), 500


@app.route('/delete_post/<int:post_id>', methods=['POST'])
def delete_post(post_id):
    if 'username' not in session:
        return jsonify({'error': 'User not logged in'}), 401

    username = session['username']
    role = db.get_user_role(username)
    post = db.get_post(post_id)  # Ensure this method returns the post with the author's information

    if post is None:
        return jsonify({'error': 'Post not found'}), 404

    if role in ['admin', 'staff'] or post['author'] == username:
        if db.delete_post(post_id):
            return jsonify({'message': 'Post deleted successfully'}), 200
        else:
            return jsonify({'error': 'Failed to delete post'}), 500
    else:
        return jsonify({'error': 'Permission denied'}), 403


@app.route('/edit_post/<int:post_id>', methods=['POST'])
def edit_post(post_id):
    if 'username' not in session:
        return jsonify({'error': 'User not logged in'}), 401

    role = db.get_user_role(session['username'])
    post = db.get_post(post_id)

    if post is None:
        return jsonify({'error': 'Post not found'}), 404

    if role in ['admin', 'staff'] or post['author'] == session['username']:
        data = request.get_json()
        title = data['title']
        content = data['content']
        if db.update_post(post_id, title, content):
            return jsonify({'message': 'Post updated successfully'}), 200
        else:
            return jsonify({'error': 'Failed to update post'}), 500
    else:
        return jsonify({'error': 'Permission denied'}), 403


@app.route('/delete_comment/<int:comment_id>', methods=['POST'])
def delete_comment(comment_id):
    if 'username' not in session:
        return jsonify({'error': 'User not logged in'}), 401

    username = session['username']
    role = db.get_user_role(username)
    comment = db.get_comment(comment_id)  # Ensure this method returns the comment with the author's information

    if comment is None:
        return jsonify({'error': 'Comment not found'}), 404

    if role in ['admin', 'staff'] or comment['author'] == username:
        if db.delete_comment(comment_id):
            return jsonify({'message': 'Comment deleted successfully'}), 200
        else:
            return jsonify({'error': 'Failed to delete comment'}), 500
    else:
        return jsonify({'error': 'Permission denied'}), 403


@app.route('/logout')
def logout():
    # Clearing a session
    session.clear()
    # Redirect to home page
    return redirect(url_for('index'))

# login page
@app.route("/login")
def login():
    return render_template("login.jinja")

# handles a post request when the user clicks the log in button
@app.route("/login/user", methods=["POST"])
def login_user():
    # Check if the request is in JSON format
    if not request.is_json:
        abort(404)

    # Get username and password from request
    username = request.json.get("username")
    password = request.json.get("password")

    # Calling database functions to check users
    user = db.get_user(username)
    if user is None:
        return "Error: User does not exist!"

    # Verify password, check hash with bcrypt
    if not bcrypt.checkpw(password.encode('utf-8'), user.password):  # Remove encoding of user.password
        return "Error: Password does not match!"
    session['username'] = username  # Storing usernames in the session
    # Login successfully, return the corresponding URL
    return url_for('home', username=username)
# handles a get request to the signup page
@app.route("/signup")
def signup():
    return render_template("signup.jinja")

# handles a post request when the user clicks the signup button
@app.route("/signup/user", methods=["POST"])
def signup_user():
    if not request.is_json:
        abort(404)
    username = request.json.get("username")
    password = request.json.get("password")

    # Generate key pairs
    public_key, private_key = generate_key_pair()

    if db.get_user(username) is None:
        db.insert_user(username, password, public_key)  # Make sure the insert_user function has been updated to accept public keys

        # Make sure the catalogue exists
        target_folder = safe_join(current_app.root_path, 'use_private_key')
        if not os.path.exists(target_folder):
            os.makedirs(target_folder)

        # Storing the private key in a file
        private_key_path = safe_join(target_folder, username + '.key')
        with open(private_key_path, 'w') as key_file:
            key_file.write(private_key)

        return "User registered successfully!"
    return "Error: User already exists!"

def signup_admin():
    admin_username = 'admin'
    admin_password = 'adminpassword'
    admin_role = 'admin'

    # Generate key pairs
    public_key, private_key = generate_key_pair()

    if db.get_user(admin_username) is None:
        db.insert_admin(admin_username, admin_password, public_key, admin_role)

        # Make sure the catalogue exists
        target_folder = safe_join(current_app.root_path, 'use_private_key')
        if not os.path.exists(target_folder):
            os.makedirs(target_folder)

        # Storing the private key in a file
        private_key_path = safe_join(target_folder, admin_username + '.key')
        with open(private_key_path, 'w') as key_file:
            key_file.write(private_key)

        return "User registered successfully!"
    return "Error: User already exists!"
# handler when a "404" error happens
@app.errorhandler(404)
def page_not_found(_):
    return render_template('404.jinja'), 404

# home page, where the messaging app is
@app.route("/home")
def home():
    if 'username' not in session:
        return redirect(url_for('login'))
    return render_template("home.jinja", username=session['username'])


@app.route('/add_friend', methods=['POST'])
def add_friend():
    data = request.get_json()
    username = data.get('username')
    friendname = data.get('friendname')

    if not username or not friendname:
        return jsonify({'error': 'Invalid data provided'}), 400

    if username == friendname:
        return jsonify({'error': 'Cannot send friend request to yourself'}), 400

    with DBSession() as session:
        user = session.query(User).filter_by(username=username).first()
        friend = session.query(User).filter_by(username=friendname).first()

        if not user or not friend:
            return jsonify({'error': 'User or friend not found'}), 404

        # Check for any existing friend request or block status
        existing_request = session.query(FriendRequest).filter(
            ((FriendRequest.sender_username == username) & (FriendRequest.receiver_username == friendname) |
             (FriendRequest.sender_username == friendname) & (FriendRequest.receiver_username == username))
        ).first()

        if existing_request:
            if existing_request.status == 'blocked':
                return jsonify({'error': 'Friend request cannot be sent. User is blocked.'}), 403
            elif existing_request.status == 'accepted':
                return jsonify({'error': 'You are already friends'}), 400
            elif existing_request.status == 'pending':
                return jsonify({'error': 'Friend request already sent'}), 400
            elif existing_request.status == 'rejected':
                # Optionally delete rejected request here if needed
                session.delete(existing_request)
                session.commit()

        # Create and add the new friend request
        new_request = FriendRequest(
            sender_username=username,
            receiver_username=friendname,
            status='pending'
        )
        session.add(new_request)
        session.commit()

        return jsonify({'message': 'Friend request sent successfully'}), 200

@app.route('/update_friend_status', methods=['POST'])
def update_friend_status():
    data = request.get_json()
    username = data.get('username')
    friendname = data.get('friendname')
    new_status = data.get('status')

    if not username or not friendname or new_status not in ['rejected', 'blocked', 'unblocked']:
        return jsonify({'error': 'Invalid data provided'}), 400

    with DBSession() as session:
        friend_request = session.query(FriendRequest).filter(
            ((FriendRequest.sender_username == username) & (FriendRequest.receiver_username == friendname)) |
            (FriendRequest.sender_username == friendname) & (FriendRequest.receiver_username == username)
        ).first()

        if friend_request:
            # If 'unblocked', we reset the status to a default non-blocking status, e.g., 'pending'
            if new_status == 'unblocked':
                friend_request.status = 'pending'
            else:
                friend_request.status = new_status
                # Handle friendship removal for 'rejected' and 'blocked' status
                if new_status in ['rejected', 'blocked']:
                    user = session.query(User).filter_by(username=username).first()
                    friend = session.query(User).filter_by(username=friendname).first()
                    if friend in user.friends:
                        user.friends.remove(friend)
                        friend.friends.remove(user)
            session.commit()
            return jsonify({'message': f'Friend request status updated to {new_status}'}), 200
        else:
            if new_status == 'blocked':
                # Create a new friend request with status 'blocked'
                new_request = FriendRequest(
                    sender_username=username,
                    receiver_username=friendname,
                    status='blocked'
                )
                session.add(new_request)
                session.commit()
                return jsonify({'message': 'Friend successfully blocked'}), 200
            return jsonify({'error': 'Friend request not found'}), 404


@app.route('/friend_requests/respond/<int:request_id>', methods=['POST'])
def respond_to_friend_request(request_id):
    data = request.get_json()
    action = data.get('action')
    if action not in ['accept', 'reject']:
        return jsonify({'error': 'Invalid action'}), 400

    with DBSession() as session:
        friend_request = session.query(FriendRequest).get(request_id)
        if not friend_request:
            return jsonify({'error': 'Friend request not found'}), 404

        if action == 'accept':
            friend_request.status = 'accepted'
            # Add to both parties' buddy lists
            user = session.query(User).filter_by(username=friend_request.receiver_username).first()
            friend = session.query(User).filter_by(username=friend_request.sender_username).first()
            if user and friend:
                user.friends.append(friend)
                friend.friends.append(user)
        elif action == 'reject':
            friend_request.status = 'rejected'

        # session.delete(friend_request)  # Delete this friend request
        session.commit()

        return jsonify({'message': 'Friend request ' + action}), 200

@app.route('/friends/<username>', methods=['GET'])
def show_friends(username):
    with DBSession() as session:
        user = session.query(User).filter_by(username=username).first()
        if not user:
            print(f"User {username} not found")  # debug output
            return jsonify({'error': 'User not found'}), 404

        friends = [{'username': friend.username, 'status': friend.status, 'role': friend.role} for friend in user.friends]
        print(f"Friends of {username}: {friends}")  # debug output
        return jsonify({'friends': friends}), 200

@app.route('/friend_requests', methods=['GET'])
def get_friend_requests():
    username = session.get('username')  # Using Flask's session
    if not username:
        return jsonify({'error': 'No user logged in'}), 401

    with DBSession() as db_session:  # Using Aliases to Avoid Naming Conflicts
        friend_requests = db_session.query(FriendRequest).filter(
            (FriendRequest.receiver_username == username) |
            (FriendRequest.sender_username == username)
        ).all()
        return jsonify([{
            'id': fr.id,
            'sender_username': fr.sender_username,
            'receiver_username': fr.receiver_username,
            'status': fr.status
        } for fr in friend_requests])


@app.route('/friend_requests/<int:request_id>', methods=['POST'])
def handle_friend_request(request_id):
    data = request.get_json()
    action = data.get('action')  # 'accept' or 'reject'

    with DBSession() as session:
        friend_request = session.query(FriendRequest).get(request_id)
        if not friend_request:
            return jsonify({'error': 'Friend request not found'}), 404

        if action == 'accept':
            friend_request.status = 'accepted'
        elif action == 'reject':
            friend_request.status = 'rejected'

        session.commit()
        return jsonify({'status': 'success', 'action': action, 'request_id': request_id})

def generate_key_pair():
    # Generate key pairs
    private_key = rsa.generate_private_key(
        public_exponent=65537,
        key_size=2048,
    )
    public_key = private_key.public_key()

    # Private key serialisation
    pem_private_key = private_key.private_bytes(
        encoding=serialization.Encoding.PEM,
        format=serialization.PrivateFormat.PKCS8,
        encryption_algorithm=serialization.NoEncryption()
    )

    # Public Key Serialisation
    pem_public_key = public_key.public_bytes(
        encoding=serialization.Encoding.PEM,
        format=serialization.PublicFormat.SubjectPublicKeyInfo
    )

    return pem_public_key.decode('utf-8'), pem_private_key.decode('utf-8')

@app.route('/public_key/<username>', methods=['GET'])
def get_public_key(username):
    user = db.get_user(username)
    if user is None:
        return jsonify({'error': 'User not found'}), 404
    return jsonify({'public_key': user.public_key})

@app.route('/get_private_key/<username>', methods=['GET'])
def get_private_key(username):
    target_folder = safe_join(app.root_path, 'use_private_key')
    private_key_path = safe_join(target_folder, username + '.key')
    if os.path.exists(private_key_path):
        with open(private_key_path, 'r') as key_file:
            private_key = key_file.read()
            return jsonify({'private_key': private_key})
    else:
        return jsonify({'error': 'Private key not found'}), 404

@app.route('/get_user_role/<username>')
def get_role(username):
    role = db.get_user_role(username)
    if role:
        return jsonify({'username': username, 'role': role}), 200
    else:
        return jsonify({'error': 'User not found'}), 404

if __name__ == '__main__':
    # with DBSession() as session:
    #     db.initialize_rooms(session)
    # app.run(debug=True, ssl_context=('./localhost+2.pem', './localhost+2-key.pem'))#https
    with app.app_context():
        print(signup_admin())  # Output registration results
    socketio.run(app)