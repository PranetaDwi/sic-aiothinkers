import pandas as pd
import pymongo
import pandas as pd
from pymongo import MongoClient

# Baca CSV
df = pd.read_csv('kasus-kriminal.csv')

# Koneksi ke MongoDB
client = MongoClient('mongodb://localhost:27017/')
db = client['indonesia']
collection = db['kriminal']

client = pymongo.MongoClient("mongodb+srv://neta_sic:neta_sic@backenddb.rfmwzg6.mongodb.net/?retryWrites=true&w=majority&appName=BackendDB")
db = client["locations"]

collection = db["kasus_kriminals"]

data = df.to_dict(orient='records')
collection.insert_many(data)

print("Data berhasil diimpor ke MongoDB!")