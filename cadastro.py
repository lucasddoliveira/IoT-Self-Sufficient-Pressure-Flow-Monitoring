import paho.mqtt.client as mqtt
import mysql.connector
import json
from datetime import datetime, timedelta
import pytz
import time
import threading

messagesCounter = 0
tensao = []
fluxo = []
pressao = []

def on_message(client, userdata, message):
    global messagesCounter
    global tensao
    global fluxo
    global pressao
    messagesCounter += 1

    json_data = message.payload.decode("utf-8")  # Decodifica a mensagem em UTF-8
    data = json.loads(json_data)  # Deserializa o JSON em uma estrutura de dados do Python
    print(data)

    tensao += data['tensao'].split(",")
    fluxo += data['fluxo'].split(",")
    pressao += data['pressao'].split(",")

    if(messagesCounter == 10):
        fuso_horario = pytz.timezone('America/Sao_Paulo')

        hora_atual = datetime.now(fuso_horario)
        dados = []
        array_60_minutos = []

        for i in range(60):
            tempo = hora_atual - timedelta(minutes=61-i)
            array_60_minutos.append(tempo.strftime('%Y-%m-%d %H:%M:%S'))

        for a, b, c, d in zip(array_60_minutos, pressao, fluxo, tensao):
            subarray = [a, b, c, d]
            dados.append(subarray)

        cnx = mysql.connector.connect(
          host="localhost",
          user="root",
          password="senha",
          database="ESP32"
        )

        cursor = cnx.cursor()
        # print(dados)
        for i in dados:
           cursor.execute('INSERT INTO dados (horario, pressao,vazao,tensao) VALUES (%s, %s, %s, %s)', i)

        cnx.commit()

        #print("cheguei5")

        # lendo os dados
        cursor.execute("""
        SELECT * FROM dados;
        """)

        for linha in cursor.fetchall():
            print(linha)

        cursor.close()
        cnx.close()

        messagesCounter = 0
        tensao = []
        fluxo = []
        pressao = []

def on_disconnect(client, userdata, rc):
    print("Conexão perdida. Tentando reconectar...")
    time.sleep(5)  # Aguarda 5 segundos antes de tentar reconectar
    client.reconnect()

def alert():
    while True:
        print('SERVER ON')
        time.sleep(20)

# Configurações do broker MQTT
mqtt_server = "broker.emqx.io"
mqtt_port = 1883
mqtt_topic = "leituras_ESP32_08112001"

# Criação de um cliente MQTT
client = mqtt.Client()
client.on_message = on_message

client.on_disconnect = on_disconnect

thread = threading.Thread(target=alert)
thread.daemon = True  # Define a thread como um daemon para encerrar junto com o programa principal
thread.start()

# Loop de reconexão
while True:
    try:
        # Conecta-se ao broker MQTT
        client.connect(mqtt_server, port=mqtt_port)
        client.subscribe(mqtt_topic)
        # Mantém a conexão e lida com as mensagens recebidas
        client.loop_forever()
    except KeyboardInterrupt:
        # Encerra o loop se o usuário pressionar Ctrl+C
        break