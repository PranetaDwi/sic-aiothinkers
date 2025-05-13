import streamlit as st
import pandas as pd
import pymongo
import pydeck as pdk
import gridfs
from io import BytesIO
from urllib.error import URLError
from datetime import datetime
import time
from pymongo import DESCENDING
import requests
import streamlit.components.v1 as components
import speech_recognition as sr
import io

st.set_page_config(page_title="Dashboard", layout="wide")

if 'device_id' not in st.session_state:
    st.session_state['device_id'] = None


if st.session_state['device_id'] is None:
    st.warning("Silahkan login dahulu untuk melanjutkan")
    st.stop() 

client = pymongo.MongoClient("mongodb+srv://neta_sic:neta_sic@backenddb.rfmwzg6.mongodb.net/?retryWrites=true&w=majority&appName=BackendDB")
db = client["locations"]

realtime_location_collection = db["realtime_location"]
vulnerable_location_collection = db["vulnerable_location"]

# Styling
st.markdown("""
    <style>
        .stMainBlockContainer {
            max-width: 95rem;
            padding-top: 5rem;
        }
    </style>
""", unsafe_allow_html=True)

def findDetailLocation(lat, lon):
    url = "https://nominatim.openstreetmap.org/reverse"
    params = {"lat": latest_data["lat"], "lon": latest_data["lon"], "format": "json"}
    headers = {"User-Agent": "MyApp"}

    r = requests.get(url, params=params, headers=headers)
    return r.json().get("display_name")

latest_data = realtime_location_collection.find_one(
    {"device_id": st.session_state["device_id"]},
    sort=[("timestamp", DESCENDING)]
)

# Simulasi data
latitude = latest_data["lat"]
longitude = latest_data["lon"]
address = findDetailLocation(latitude, longitude)

fs = gridfs.GridFS(db)

audio_files = list(fs.find())


audio_cursor = fs.find({"device_id": st.session_state["device_id"]}) \
                .sort("timestamp", DESCENDING).limit(1)

audio_list = list(audio_cursor)


def dashboardInitial():
    # TITLE
    st.title("Dashboard")
    st.markdown("---")

    st.subheader("Current Location")

def card1():
    st.components.v1.html(f"""
        <head>
            <link href="https://fonts.googleapis.com/css2?family=Inter&display=swap" rel="stylesheet">
            <style>
                body {{
                    font-family: 'Inter', sans-serif;
                }}
            </style>
        </head>

        <div style="display: flex; flex-wrap: wrap; gap: 2rem; box-shadow: 0 4px 12px rgba(0, 0, 0, 0.1); 
                    padding: 24px; border-radius: 16px; background-color: #fff;
                    font-family: 'Inter', sans-serif;align-items: center;">

            <div style="flex: 1; min-width: 300px;">
                <h3>Realtime Location</h3>
                <iframe src="https://maps.google.com/maps?q={latitude},{longitude}&z=15&output=embed" 
                        width="100%" height="300" style="border:1px solid #ccc; border-radius:12px;"></iframe>
            </div>

            <div style="flex: 1; min-width: 300px;">
                <h3>Latest Location</h3>
                <p><strong>Latitude:</strong> {latitude}</p>
                <p><strong>Longitude:</strong> {longitude}</p>
                <p><strong>Address:</strong> {address}</p>
            </div>

        </div>
    """, height=430)

    st.markdown("<h4 style='margin-bottom: 0; font-size: 20px;'>Latest Voice Audio</h4>", unsafe_allow_html=True)

    # Pemutar audio Streamlit di luar HTML
    if audio_list:
        audio_file = audio_list[0]
        audio_bytes = fs.get(audio_file._id).read()
        st.audio(audio_bytes, format="audio/mp3")

        formatted_time = audio_file.uploadDate.strftime("%Y-%m-%d %H:%M")

        st.markdown(f"<p style='margin-top: 8px;'>Updated at: {formatted_time}</p>", unsafe_allow_html=True)
        
    else:
        st.warning("Tidak ada file audio yang ditemukan.")

    col1, col2 = st.columns([1, 1])
    with col1:
        st.button("👁️ See Detail", use_container_width=True)

    with col2:
        st.button("⛔ Nonaktif alat", use_container_width=True)

    st.markdown(f"<a href='#' style='display: inline-block; text-align: right; background-color: #FFFFFF; color: black; border-radius: 8px; margin-top: 5px; text-decoration: none; width:99%;'>If this activation is an error, click stop activation</a>", unsafe_allow_html=True)

def card2():
    pipeline = [
        {"$match": {"device_id": st.session_state["device_id"]}},  # Filter device_id tertentu
        {"$sort": {"timestamp": 1}},  # Urutkan dari yang paling lama ke yang paling baru
        {"$group": {
            "_id": "$activation_id",         # Grup berdasarkan activation_id
            "oldest": {"$first": "$$ROOT"}   # Ambil dokumen pertama (yang terlama)
        }},
        {"$replaceRoot": {"newRoot": "$oldest"}}  # Kembalikan struktur ke dokumen asli
    ]

    results = list(realtime_location_collection.aggregate(pipeline))
    for idx, item in enumerate(results):
        timestamp = item.get("timestamp", "-")
        lat = item.get("lat", "-")
        lon = item.get("lon", "-")
        activation_id = item.get("activation_id", "-")

        detail_location = findDetailLocation(lat, lon)
        formatted_timestamp = timestamp.strftime("%Y-%m-%d %H:%M") if timestamp != "-" else "-"

        # form per item
        with st.form(f"form_{idx}", clear_on_submit=True):
            col1, col2 = st.columns([6, 1])

            with col1:
                st.markdown(f"""
                    <div style="display: flex; flex-direction: column; justify-content: center; height: 100%;">
                        <div style="font-weight: 600; font-size: 14px;">{formatted_timestamp}</div>
                        <div style="color: #666; font-size: 13px;">{detail_location}</div>
                        <div style="color: #666; font-size: 13px;">{activation_id}</div>
                    </div>
                """, unsafe_allow_html=True)

            with col2:
                st.write("")  # spacer
                st.form_submit_button("Pilih History Aktivasi", on_click=lambda d_id=activation_id: set_session(activation_id))

def set_session(activation_id):
    st.session_state["selected_device_activation"] = activation_id
    st.session_state["should_rerun"] = True 

if st.session_state.get("should_rerun"):
    del st.session_state["should_rerun"]
    st.rerun()

def cardLocationDetailActivity():

    latest_data_by_activation = realtime_location_collection.find_one(
        {
            "device_id": st.session_state["device_id"],
            "activation_id": st.session_state["selected_device_activation"]
        },
        sort=[("timestamp", DESCENDING)]
    )


    # Simulasi data
    latitude_act = latest_data_by_activation["lat"]
    longitude_act = latest_data_by_activation["lon"]
    timestamp = latest_data_by_activation["timestamp"]
    address_act = findDetailLocation(latitude_act, longitude_act)

    url = "https://api.openweathermap.org/data/2.5/weather?"
    params = {"lat": latitude_act, "lon": longitude_act, "appid": "8178d007e66b761f30298db1fc1dd3af"}
    headers = {"User-Agent": "MyApp"}

    req = requests.get(url, params=params, headers=headers)

    formatted_timestamp = timestamp.strftime("%Y-%m-%d %H:%M") if timestamp != "-" else "-"

    st.title("Detail Activation")
    st.markdown("---")

    st.components.v1.html(f"""
        <head>
            <link href="https://fonts.googleapis.com/css2?family=Inter&display=swap" rel="stylesheet">
            <style>
                body {{
                    font-family: 'Inter', sans-serif;
                }}
                .container {{
                    display: flex;
                    flex-wrap: wrap;
                    gap: 2rem;
                    box-shadow: 0 4px 12px rgba(0, 0, 0, 0.1);
                    padding: 24px;
                    border-radius: 16px;
                    background-color: #fff;
                    align-items: center;
                }}
                .section-map {{
                    flex: 2;
                    min-width: 300px;
                }}
                .section-weather {{
                    flex: 1;
                    min-width: 300px;
                }}
                .weather-box {{
                    display: grid;
                    grid-template-columns: repeat(2, 1fr);
                    gap: 12px;
                    padding: 16px;
                    border-radius: 12px;
                    margin-top: 12px;
                }}
                .weather-box div {{
                    background-color: #ffffff;
                    padding: 10px;
                    border-radius: 8px;
                    text-align: center;
                    box-shadow: 0 1px 3px rgba(0,0,0,0.1);
                }}
            </style>
        </head>

        <div class="container">
            <div class="section-map">
                <h3>Realtime Location</h3>
                <iframe src="https://maps.google.com/maps?q={latitude_act},{longitude_act}&z=15&output=embed" 
                        width="100%" height="330" style="border:1px solid #ccc; border-radius:12px;"></iframe>
            </div>

            <div class="section-weather">
                <h3>Weather</h3>
                <div class="weather-box">
                    <div>
                        <strong>Clouds</strong><br>
                        {req.json()["weather"][0]["description"]}
                    </div>
                    <div>
                        <strong>Temperature</strong><br>
                        {req.json()["main"]["temp_min"]}
                    </div>
                    <div>
                        <strong>Humidity</strong><br>
                        {req.json()["main"]["humidity"]}
                    </div>
                    <div>
                        <strong>Pressure</strong><br>
                        {req.json()["main"]["pressure"]}
                    </div>
                    <div>
                        <strong>Wind Speed</strong><br>
                        {req.json()["wind"]["speed"]}
                    </div>
                    <div>
                        <strong>Visibility</strong><br>
                        {req.json()["visibility"]}
                    </div>
                </div>
                <p style="margin-top: 8px; font-size: 14px;">Updated at: {formatted_timestamp}</p>
            </div>
        </div>
    """, height=460)

def transcribe_audio(audio_data):
    recognizer = sr.Recognizer()
    audio_file = io.BytesIO(audio_data)

    with sr.AudioFile(audio_file) as source:
        audio = recognizer.record(source)

    try:
        return recognizer.recognize_google(audio)
    except sr.UnknownValueError:
        return "Tidak bisa mengenali suara."
    except sr.RequestError as e:
        return f"Error saat permintaan: {e}"


def detailLocationAudioList():
    col1, col2 = st.columns([1, 1])
    with col1:
        pipeline = [
            {"$match": {
                "device_id": st.session_state["device_id"],
                "activation_id": st.session_state["selected_device_activation"]
            }},
            {"$sort": {"timestamp": 1}},
            {"$group": {"_id": "$activation_id", "oldest": {"$first": "$$ROOT"}}},
            {"$replaceRoot": {"newRoot": "$oldest"}}
        ]

        results = list(realtime_location_collection.aggregate(pipeline))

        st.markdown("<h4 style='font-family: Inter, sans-serif;margin-top:10px'>Location List</h4>", unsafe_allow_html=True)

        for idx, item in enumerate(results):
            timestamp = item.get("timestamp", "-")
            lat = item.get("lat", "-")
            lon = item.get("lon", "-")
            activation_id = item.get("activation_id", "-")

            detail_location = findDetailLocation(lat, lon)
            
            # Format waktu aman
            formatted_timestamp = "-"
            if timestamp != "-" and hasattr(timestamp, "strftime"):
                formatted_timestamp = timestamp.strftime("%Y-%m-%d %H:%M")
            
            with st.form(f"loc_{idx}", clear_on_submit=True):
                # Buat dua kolom DI DALAM form (boleh, karena bukan nested columns)
                form_col1, form_col2 = st.columns([6, 1])

                with form_col1:
                    st.markdown(f"""
                        <div style="display: flex; flex-direction: column; justify-content: center; height: 100%;">
                            <div style="font-weight: 600; font-size: 14px;">{formatted_timestamp}</div>
                            <div style="color: #666; font-size: 13px;">{detail_location}</div>
                        </div>
                    """, unsafe_allow_html=True)

                with form_col2:
                    st.write("")  # spacer
                    st.form_submit_button("Maps", on_click=lambda d_id=activation_id: set_session(d_id))

    with col2:

        if "transkript" not in st.session_state:
            st.session_state.transkript = "no"

        # Header
        st.markdown("<h4 style='font-family: Inter, sans-serif;'>Record List</h4>", unsafe_allow_html=True)

        # Kontainer scrollable
        with st.container():
            # Ambil data audio berdasarkan activation_id, urutkan berdasarkan timestamp
            audios = list(fs.find({"activation_id": activation_id}).sort("timestamp", 1))

            for i, audio in enumerate(audios):
                audio_data = audio.read()

                col1, col2 = st.columns([5, 1])

                with col1:
                    st.audio(audio_data, format="audio/mp3")

                with col2:
                    if st.button("Transkript", key=f"transkript-btn-{i}"):
                        recognizer = sr.Recognizer()

                        with sr.AudioFile(io.BytesIO(audio_data)) as source:
                            st.write("🛠️ Mengenali suara...")
                            audio = recognizer.record(source)

                            try:
                                hasil = recognizer.recognize_google(audio, language="id-ID")
                                vote(hasil)
                            except sr.UnknownValueError:
                                st.error("❌ Suara tidak dikenali.")
                            except sr.RequestError:
                                st.error("❌ Gagal terhubung ke Google Speech API.")

@st.dialog("Transkript Suara")
def vote(transkript):
    st.write(transkript)

def backButton():
    if st.button("< Back"):
        keys_to_remove = ["selected_device_activation"]
        for key in keys_to_remove:
            if key in st.session_state:
                del st.session_state[key]
        st.rerun()



if 'device_id' in st.session_state and 'selected_device_activation' in st.session_state:
    cardLocationDetailActivity()
    detailLocationAudioList()
    backButton()
    
elif 'device_id' in st.session_state:
    dashboardInitial()
    card1()
    st.subheader("Activation History")
    card2()





    



