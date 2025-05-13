from flask import Flask, request
from pymongo import MongoClient
import gridfs
import datetime
from flask_mail import Mail, Message
import unicodedata
import os
from dotenv import load_dotenv

load_dotenv()

app = Flask(__name__)

# Konfigurasi Flask-Mail
# Konfigurasi Flask-Mail via .env
app.config['MAIL_SERVER'] = os.getenv('MAIL_SERVER')
app.config['MAIL_PORT'] = int(os.getenv('MAIL_PORT'))
app.config['MAIL_USERNAME'] = os.getenv('MAIL_USERNAME')
app.config['MAIL_PASSWORD'] = os.getenv('MAIL_PASSWORD')
app.config['MAIL_USE_TLS'] = os.getenv('MAIL_USE_TLS') == 'True'
app.config['MAIL_USE_SSL'] = os.getenv('MAIL_USE_SSL') == 'True'

mail = Mail(app)

# Koneksi ke MongoDB
client = MongoClient("mongodb+srv://neta_sic:neta_sic@backenddb.rfmwzg6.mongodb.net/?retryWrites=true&w=majority&appName=BackendDB")
db = client["locations"]
users_collection = db["users"]
realtime_collection = db["realtime_location"]
vulnerable_location_collection = db["vulnerable_location"]

fs = gridfs.GridFS(db)

# Bersihkan string dari karakter non-ASCII
def clean_text(text):
    if not text:
        return ""
    return unicodedata.normalize("NFKD", text).replace('\xa0', ' ').strip()

# Kirim Email
def send_email(device_id, lat, lon, email_receiver):
    try:
        msg = Message(
            subject="Peringatan Alat Aktif",
            sender=app.config['MAIL_USERNAME'],
            recipients=[email_receiver],
            body=f"""
Sistem IoT aktif dari device ID: {clean_text(device_id)}

Lokasi:
Latitude: {clean_text(lat)}
Longitude: {clean_text(lon)}

Waktu: {datetime.datetime.utcnow().strftime('%Y-%m-%d %H:%M:%S')} UTC
""".strip()
        )
        mail.send(msg)
        print("Email berhasil dikirim.")
    except Exception as e:
        print("Gagal mengirim email:", e)

# Register API
@app.route('/register', methods=['POST'])
def register():
    email = clean_text(request.form.get('email'))
    password = clean_text(request.form.get('password'))
    device_id = clean_text(request.form.get('device_id'))

    if not email:
        return {'message': 'Email tidak boleh kosong'}, 400
    if not password:
        return {'message': 'Password tidak boleh kosong'}, 400
    if not device_id:
        return {'message': 'Device ID tidak boleh kosong'}, 400

    users_collection.insert_one({
        'device_id': device_id,
        'email': email,
        'password': password,
        'timestamp': datetime.datetime.utcnow()
    })

    return {'message': 'Berhasil mengirim data', 'Email': email, 'device_id': device_id}, 200

# Upload audio
@app.route('/upload', methods=['POST'])
def upload_audio():
    if 'file' not in request.files:
        return {'message': 'File tidak ditemukan'}, 400

    file = request.files['file']
    device_id = clean_text(request.form.get('device_id'))

    if not device_id:
        return {'message': 'Device ID tidak boleh kosong'}, 400
    if file.filename == '':
        return {'message': 'Nama file kosong'}, 400

    file_id = fs.put(file, device_id=device_id, filename=file.filename, content_type=file.content_type, uploadDate=datetime.datetime.utcnow())
    return {'message': 'Upload berhasil', 'device_id': device_id, 'file_id': str(file_id)}, 200

# Lokasi rawan
@app.route('/vulnerable-location', methods=['POST'])
def post_vulnerable_location():
    device_id = clean_text(request.form.get('device_id'))
    lon = clean_text(request.form.get('lon'))
    lat = clean_text(request.form.get('lat'))

    if not lat:
        return {'message': 'Lat tidak boleh kosong'}, 400
    if not lon:
        return {'message': 'Lon tidak boleh kosong'}, 400
    if not device_id:
        return {'message': 'Device ID tidak boleh kosong'}, 400

    vulnerable_location_collection.insert_one({
        'device_id': device_id,
        'lat': float(lat),
        'lon': float(lon),
        'timestamp': datetime.datetime.utcnow()
    })

    send_email(device_id, lat, lon, "marimo.zx@gmail.com")

    return {'message': 'Berhasil mengirim data', 'device_id': device_id, 'lon': lon, 'lat': lat}, 200

# Lokasi real-time
@app.route('/post-location', methods=['POST'])
def post_lang_lot():
    device_id = clean_text(request.form.get('device_id'))
    lon = clean_text(request.form.get('lon'))
    lat = clean_text(request.form.get('lat'))

    if not lat:
        return {'message': 'Lat tidak boleh kosong'}, 400
    if not lon:
        return {'message': 'Lon tidak boleh kosong'}, 400
    if not device_id:
        return {'message': 'Device ID tidak boleh kosong'}, 400

    realtime_collection.insert_one({
        'device_id': device_id,
        'lat': float(lat),
        'lon': float(lon),
        'timestamp': datetime.datetime.utcnow()
    })

    return {'message': 'Berhasil mengirim data', 'device_id': device_id, 'lon': lon, 'lat': lat}, 200

if __name__ == '__main__':
    app.run(debug=True, port=5000, host="0.0.0.0")
