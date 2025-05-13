import streamlit as st
import pymongo
import datetime
import time
import streamlit.components.v1 as components

st.markdown("""
    <style>
        .stMainBlockContainer {
            max-width: 95rem;
            padding-top: 5rem;
        }
    </style>
""", unsafe_allow_html=True)


# Koneksi ke MongoDB
client = pymongo.MongoClient("mongodb+srv://neta_sic:neta_sic@backenddb.rfmwzg6.mongodb.net/?retryWrites=true&w=majority&appName=BackendDB")
db = client["locations"]
users_collection = db["users"]

# Inisialisasi session state jika belum ada
if 'show_form' not in st.session_state:
    st.session_state['show_form'] = 'login'  # Default ke halaman login

def button_login_register():
    # Tombol login dan register
    col1, col2, col3 = st.columns([1, 1, 1])

    with col1:
        st.markdown("<div style='text-align: center;'>", unsafe_allow_html=True)
        if st.button("🔐 Login", use_container_width=True):
            st.session_state['show_form'] = 'login'
            st.rerun()
        st.markdown("</div>", unsafe_allow_html=True)

    with col2:
        st.markdown("<div style='text-align: center;'>", unsafe_allow_html=True)
        if st.button("📝 Register", use_container_width=True):
            st.session_state['show_form'] = 'register'
            st.rerun()
        st.markdown("</div>", unsafe_allow_html=True)

    with col3:
        st.markdown("<div style='text-align: center;'>", unsafe_allow_html=True)
        if st.button("👩‍👦 Family", use_container_width=True):
            st.session_state['show_form'] = 'family'
            st.rerun()
        st.markdown("</div>", unsafe_allow_html=True)

# Fungsi untuk menampilkan form login
def show_login_form():
    st.info("Silakan login:")
    with st.form("login_form"):
        login_email = st.text_input("Email")
        login_password = st.text_input("Password", type="password")
        login_submit = st.form_submit_button("Login")
        if login_submit:
            if not login_email or not login_password:
                st.warning("Email dan password harus diisi!")
            else:
                # Verifikasi login dengan database
                user = users_collection.find_one({'email': login_email, 'password': login_password})
                if user:
                    # Jika login berhasil, set session state dan ubah halaman
                    st.session_state['show_form'] = 'home'
                    st.session_state['device_id'] = user['device_id']
                    st.session_state['email'] = login_email
                    st.success(f"Selamat datang, {login_email}!")
                    st.rerun()
                else:
                    st.error("Email atau password salah!")

# Fungsi untuk menampilkan form register
def show_register_form():
    st.info("Buat akun baru:")
    with st.form("register_form"):
        email = st.text_input("Email")
        password = st.text_input("Password", type="password")
        device_id = st.text_input("ID Alat")
        device_key = st.text_input("Sandi Alat")
        age = st.text_input("Usia")
        gender = st.text_input("Jenis Kelamin")
        telepon = st.text_input("Telepon")
        register_submit = st.form_submit_button("Register")
        if register_submit:
            if not email:
                st.warning("Email harus diisi!")
            elif not password:
                st.warning("Password harus diisi!")
            elif not device_id:
                st.warning("ID Alat harus diisi!")
            elif not device_key:
                st.warning("Sandi Alat harus diisi!")
            else:
                # Cek apakah email sudah terdaftar
                existing_user = users_collection.find_one({'email': email})
                if existing_user:
                    st.warning("Email sudah terdaftar!")
                else:
                    # Registrasi berhasil
                    result = users_collection.insert_one({
                        'device_id': device_id,
                        'device_key': device_key,
                        'email': email,
                        'password': password,
                        'age': age,
                        'gender': gender,
                        'telepon': telepon,
                        "key_option": "tolong",
                        'status': "Pemilik",
                        'timestamp': datetime.datetime.utcnow()
                    })
                    st.success("Registrasi berhasil! Silakan login.")
                    time.sleep(2)
                    st.session_state['show_form'] = 'login'
                    st.rerun()

def show_family_form():
    st.info("Silakan masukkan email dan device id:")
    with st.form("device_id_form"):
        email = st.text_input("Email")
        device_id = st.text_input("Device ID")
        login_submit = st.form_submit_button("Submit")
        if login_submit:
            if not email:
                st.warning("Email diisi!")
            elif not device_id:
                st.warning("Id Device diisi!")
            else:
                user = users_collection.find_one({'email': email, 'device_id': device_id, 'status': "Family" })
                if user:
                    st.session_state['email'] = email
                    st.session_state['device_id'] = device_id
                    st.session_state['show_form'] = "home_family"
                    st.success(f"Selamat datang Family")
                    st.rerun()
                else:
                    st.error("Data tidak ditemukan!")


# Fungsi untuk menampilkan profil
def show_profile_header():
    device_id = st.session_state.get('device_id', 'Unknown Device')

    st.title("Profile")
    st.markdown("---")

    st.markdown(
        """
        <style>
        @import url('https://fonts.googleapis.com/css2?family=Inter:wght@400;600&display=swap');

        body {
            font-family: 'Inter', sans-serif;
            font-size: 18px;  /* Ukuran font untuk seluruh body */
        }

        .css-ffhzg2 {
            font-family: 'Inter', sans-serif;
            font-size: 18px;  /* Ukuran font untuk elemen tertentu */
        }

        /* Kamu bisa menambahkan lebih banyak styling seperti ini */
        .stMarkdown p {
            font-size: 20px;  /* Ukuran font untuk teks markdown */
        }
        </style>
        """, unsafe_allow_html=True
    )

def show_profile_account():
    user = users_collection.find_one({'email': st.session_state['email']})

    # Data
    nama = user['name']
    email = user['email']
    telepon = user['telepon']
    status = user['status']

    # Layout dengan 2 kolom
    with st.container():
        st.markdown("### Profil Pengguna")
        
        col1, col2, col3 = st.columns([1.2, 2, 1])
        
        with col1:
            st.markdown("**👤 Nama**")
            st.markdown("**✉️ Email**")
            st.markdown("**📞 Telepon**")
            st.markdown("**💫 Status**")

        with col2:
            st.write(nama)
            st.write(email)
            st.write(telepon)
            st.write(status)

        with col3:
            st.button("✏️ Edit Profil", use_container_width=True)
            if st.button("🔒 Edit Password", use_container_width=True):
                edit_password()  # pastikan fungsi ini sudah didefinisikan

    # Divider untuk pembatas
    st.divider()

def show_profile_account_for_family():
    user = users_collection.find_one({'email': st.session_state['email']})

    # Data
    nama = user['name']
    email = user['email']
    telepon = user['telepon']
    status = user['status']

    # Layout dengan 2 kolom
    with st.container():
        st.markdown("### Profil Pengguna")
        
        col1, col2, col3 = st.columns([1.2, 2, 1])
        
        with col1:
            st.markdown("**👤 Nama**")
            st.markdown("**✉️ Email**")
            st.markdown("**📞 Telepon**")
            st.markdown("**💫 Status**")

        with col2:
            st.write(nama)
            st.write(email)
            st.write(telepon)
            st.write(status)

        with col3:
            st.button("✏️ Edit Profil", use_container_width=True)

    # Divider untuk pembatas
    st.divider()


def show_profile_family():
    st.subheader("Keluarga Terdaftar")

    if st.button("👨‍👩‍👧‍👦 Tambah Keluarga"):
        addFamily()

    col1, col2, col3 = st.columns([2, 2, 1])
    with col1:
        st.markdown("**👤 Nama**")
    with col2:
        st.markdown("**📧 Email**")
    with col3:
        st.markdown("**📱 Telepon**")

    query = {"device_id": st.session_state['device_id'], "status": "Family"}
    results = users_collection.find(query)

    # Tampilkan data
    for user in results:
        name = user.get("name", "-")
        email = user.get("email", "-")
        phone = user.get("telepon", "-")
        
        with st.container():
            col1, col2, col3 = st.columns([2, 2, 1])
            with col1:
                st.write(name)
            with col2:
                st.write(email)
            with col3:
                st.write(phone)

@st.dialog("Change Password")
def edit_password():
    user = users_collection.find_one({'email': st.session_state['email']})
    st.subheader("Current Password")
    st.header(user['key_option'])

    col1, col2, col3 = st.columns([1, 1, 1])

    with col1:
        st.markdown("<div style='text-align: center;'>", unsafe_allow_html=True)
        if st.button("Ayolah", use_container_width=True):
            st.session_state['key_option'] = 'ayolah'
        st.markdown("</div>", unsafe_allow_html=True)

    with col2:
        st.markdown("<div style='text-align: center;'>", unsafe_allow_html=True)
        if st.button("Tolong", use_container_width=True):
            st.session_state['key_option'] = 'tolong'
        st.markdown("</div>", unsafe_allow_html=True)

    with col3:
        st.markdown("<div style='text-align: center;'>", unsafe_allow_html=True)
        if st.button("Please", use_container_width=True):
            st.session_state['key_option'] = 'please'
        st.markdown("</div>", unsafe_allow_html=True)

    if st.button("Update kata kunci", use_container_width=True):

        user = users_collection.find_one({'email': st.session_state['email']}) 
        if user:

            result = users_collection.update_one(
                {'email': st.session_state['email']},  # Ganti dengan email atau identifier lainnya
                {'$set': {'key_option': st.session_state['key_option']}}
            )
            if result.modified_count > 0:
                st.success(f"Pilihan '{st.session_state['key_option']}' berhasil disimpan!")
                st.rerun()
            else:
                st.error("Tidak ada perubahan yang disimpan.")
        else:
            st.error("Pengguna tidak ditemukan.")

@st.dialog("Add Family")
def addFamily():
    with st.form("add_family_form"):
        name = st.text_input("Name")
        email = st.text_input("Email")
        age = st.text_input("Usia")
        gender = st.text_input("Jenis Kelamin")
        telepon = st.text_input("Telepon")
        register_submit = st.form_submit_button("Tambahkan Keluarga")
        if register_submit:
            if not name:
                st.warning("Nama harus diisi!")
            elif not email:
                st.warning("Email harus diisi!")
            elif not age:
                st.warning("Usia harus diisi!")
            elif not gender:
                st.warning("Jenis Kelamin Alat harus diisi!")
            elif not telepon:
                st.warning("Telepon Alat harus diisi!")
            else:
                # Cek apakah email sudah terdaftar
                existing_user = users_collection.find_one({'email': email})
                if existing_user:
                    st.warning("Email sudah terdaftar!")
                else:
                    # Registrasi berhasil
                    result = users_collection.insert_one({
                        'name': name,
                        'device_id': st.session_state['device_id'],
                        'email': email,
                        'age': age,
                        'gender': gender,
                        'telepon': telepon,
                        'status': "Family",
                        'timestamp': datetime.datetime.utcnow()
                    })
                    st.success("Berhasil Menambahkan keluarga!")
                    st.rerun()

# Cek halaman berdasarkan session state
if st.session_state['show_form'] == 'login':
    button_login_register()
    # Menampilkan form login
    show_login_form()
elif st.session_state.get('show_form') == 'register':
    button_login_register()
    # Menampilkan form register
    show_register_form()
elif st.session_state.get('show_form') == 'home':
    show_profile_header()
    show_profile_account()
    show_profile_family()
elif st.session_state.get('show_form') == 'family':
    button_login_register()
    show_family_form()
elif st.session_state.get('show_form') == 'home_family':
    show_profile_header()
    show_profile_account_for_family()