import streamlit as st
import pandas as pd
import numpy as np
import pymongo
import altair as alt

st.set_page_config(page_title="Summary", layout="wide")

st.title("Summary AIOVOICE")
st.markdown("---")


st.markdown("""
    <style>
        .stMainBlockContainer {
            max-width: 95rem;
            padding-top: 5rem;
        }
    </style>
""", unsafe_allow_html=True)

client = pymongo.MongoClient("mongodb+srv://neta_sic:neta_sic@backenddb.rfmwzg6.mongodb.net/?retryWrites=true&w=majority&appName=BackendDB")
db = client["locations"]

realtime_location_collection = db["realtime_location"]
vulnerable_location_collection = db["vulnerable_location"]
crime_collection = db["kasus_kriminals"]

col1, col2, col3 = st.columns([1, 1, 1])

with col1:
    st.markdown(f"""
        <div style="text-align: center; box-shadow: 0 4px 12px rgba(0, 0, 0, 0.1); padding: 20px; border-radius: 10px; margin-bottom: 20px;">
            <h4>Total of Activation</h4>
            <p style="font-size: 30px">100</p>
        </div>
    """, unsafe_allow_html=True)

with col2:
    st.markdown(f"""
        <div style="text-align: center; box-shadow: 0 4px 12px rgba(0, 0, 0, 0.1); padding: 20px; border-radius: 10px;">
            <h4>Female User</h4>
            <p style="font-size: 30px">45</p>
        </div>
    """, unsafe_allow_html=True)

with col3:
    st.markdown(f"""
        <div style="text-align: center; box-shadow: 0 4px 12px rgba(0, 0, 0, 0.1); padding: 20px; border-radius: 10px;">
            <h4>Male User</h4>
            <p style="font-size: 30px">100</p>
        </div>
    """, unsafe_allow_html=True)


st.subheader("Activation Distribution Map")

df = pd.DataFrame(
    np.random.randn(100, 2) / [100, 100] + [-7.7956, 110.3695],
    columns=["lat", "lon"]
)
st.map(df)

st.subheader("Number of Activations by Province")

data = list(crime_collection.find({}, {'_id': 0}))
df = pd.DataFrame(data)
st.bar_chart(data=df.set_index("Provinsi"))


st.subheader("Activating Users by Age and Gender")

# Dummy data
data = {
    'Kelompok Usia': ['0-5', '6-12', '13-17', '18-24', '25-44', '45-59', '60+'] * 2,
    'Jenis Kelamin': ['Female'] * 7 + ['Male'] * 7,
    'Jumlah': [5, 9, 3, 12, 3, 5, 9, 9, 10, 12, 12, 10, 8, 5]
}

df = pd.DataFrame(data)

# Altair grouped bar chart
chart = alt.Chart(df).mark_bar().encode(
    x=alt.X('Kelompok Usia:N', title='Kelompok Usia'),
    xOffset='Jenis Kelamin:N',
    y=alt.Y('Jumlah:Q', title='Jumlah'),
    color=alt.Color('Jenis Kelamin:N', scale=alt.Scale(range=['#a3a0fb', '#fca5a5'])),
    tooltip=['Kelompok Usia', 'Jenis Kelamin', 'Jumlah']
).properties(
    width=600,
    height=400
)

st.altair_chart(chart, use_container_width=True)


