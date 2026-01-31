#include <WiFi.h>
#include <PubSubClient.h>
#include <WiFiClientSecure.h>
#include <time.h>
#include "secrets.h"
#include "DHT.h"

#define LED 25 // LED
#define LIGHT_SENSOR 36
#define ADC_MAX_INPUT 4095
#define ADC_MAX_OUTPUT 255
#define SENSOR_OFFSET 1130
#define dht_dpin 16
#define DHTTYPE DHT11 // DHT 11
#define HOSTNAME "" // Fill
#define CIUDAD "" // Fill
#define PWD "" // Fill

DHT dht(dht_dpin, DHTTYPE);

const char* ssid = ""; // Fill
const char* password = ""; // Fill


//Conexión a Mosquitto
const char MQTT_HOST[] = "iotlab.virtual.uniandes.edu.co";
const int MQTT_PORT = 8082;
//Usuario uniandes sin @uniandes.edu.co
const char MQTT_USER[] = HOSTNAME;
//Contraseña de MQTT que recibió por correo
const char MQTT_PASS[] = PWD;
const char MQTT_SUB_TOPIC[] = HOSTNAME "/";
//Tópico al que se enviarán los datos de humedad
const char MQTT_PUB_TOPIC1[] = "humedad/" CIUDAD "/" HOSTNAME;
//Tópico al que se enviarán los datos de temperatura
const char MQTT_PUB_TOPIC2[] = "temperatura/" CIUDAD "/" HOSTNAME;
//Tópico al que se enviarán los datos de luminosidad
const char MQTT_PUB_TOPIC3[] = "luminosidad/" CIUDAD "/" HOSTNAME;

#if (defined(CHECK_PUB_KEY) and defined(CHECK_CA_ROOT)) or (defined(CHECK_PUB_KEY) and defined(CHECK_FINGERPRINT)) or (defined(CHECK_FINGERPRINT) and defined(CHECK_CA_ROOT)) or (defined(CHECK_PUB_KEY) and defined(CHECK_CA_ROOT) and defined(CHECK_FINGERPRINT))
  #error "cant have both CHECK_CA_ROOT and CHECK_PUB_KEY enabled"
#endif

WiFiClientSecure net;
PubSubClient client(net);

time_t now;
unsigned long lastMillis = 0;

//Función que conecta el node a través del protocolo MQTT
//Emplea los datos de usuario y contraseña definidos en MQTT_USER y MQTT_PASS para la conexión
void mqtt_connect()
{
  //Intenta realizar la conexión indefinidamente hasta que lo logre
  while (!client.connected()) {
    Serial.print("Time: ");
    Serial.print(ctime(&now));
    Serial.print("MQTT connecting ... ");
    if (client.connect(HOSTNAME, MQTT_USER, MQTT_PASS)) {
      Serial.println("connected.");
    } else {
      Serial.println("Problema con la conexión, revise los valores de las constantes MQTT");
      Serial.print("Código de error = ");
      Serial.println(client.state());
      if ( client.state() == MQTT_CONNECT_UNAUTHORIZED ) {
        ESP.deepSleep(0);
      }
      /* Espera 5 segundos antes de volver a intentar */
      delay(5000);
    }
  }
}

void receivedCallback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Received [");
  Serial.print(topic);
  Serial.print("]: ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
}

void setup() {
  pinMode(dht_dpin, INPUT);
  pinMode(LED, OUTPUT);
  analogWrite(LED, 255);
  Serial.begin(115200);
  dht.begin();
  Serial.println();
  Serial.println();
  Serial.print("Attempting to connect to SSID: ");
  Serial.print(ssid);
  WiFi.hostname(HOSTNAME);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  //Intenta conectarse con los valores de las constantes ssid y pass a la red Wifi
  //Si la conexión falla el node se dormirá hasta que lo resetee
  while (WiFi.status() != WL_CONNECTED)
  {
    if ( WiFi.status() == WL_NO_SSID_AVAIL ) {
      Serial.print("\nProblema con la conexión, revise los valores de las constantes ssid y pass");
      ESP.deepSleep(0);
    } else if ( WiFi.status() == WL_CONNECT_FAILED ) {
      Serial.print("\nNo se ha logrado conectar con la red, resetee el node y vuelva a intentar");
      ESP.deepSleep(0);
    }
    Serial.print(".");
    delay(1000);
  }
  Serial.println("connected!");

  //Sincroniza la hora del dispositivo con el servidor SNTP (Simple Network Time Protocol)
  Serial.print("Setting time using SNTP");
  configTime(-5 * 3600, 0, "pool.ntp.org", "time.nist.gov");
  now = time(nullptr);
  while (now < 1510592825) {
    delay(500);
    Serial.print(".");
    now = time(nullptr);
  }
  Serial.println("done!");
  struct tm timeinfo;
  gmtime_r(&now, &timeinfo);
  //Una vez obtiene la hora, imprime en el monitor el tiempo actual
  Serial.print("Current time: ");
  Serial.print(asctime(&timeinfo));

  #ifdef CHECK_CA_ROOT
    net.setCACert(digicert);
  #endif
  #ifdef CHECK_PUB_KEY
    BearSSL::PublicKey key(pubkey);
    net.setKnownKey(&key);
  #endif
  #ifdef CHECK_FINGERPRINT
    net.setFingerprint(fp);
  #endif
  #if (!defined(CHECK_PUB_KEY) and !defined(CHECK_CA_ROOT) and !defined(CHECK_FINGERPRINT))
    net.setInsecure();
  #endif

  //Llama a funciones de la librería PubSubClient para configurar la conexión con Mosquitto
  client.setServer(MQTT_HOST, MQTT_PORT);
  client.setCallback(receivedCallback);
  //Llama a la función de este programa que realiza la conexión con Mosquitto
  mqtt_connect();
}

void loop() {
 // Check if a client has connected
  analogWrite(LED, 0);

  if (WiFi.status() != WL_CONNECTED)
  {
    Serial.print("Checking wifi");
    while (WiFi.waitForConnectResult() != WL_CONNECTED)
    {
      WiFi.begin(ssid, password);
      Serial.print(".");
      delay(10);
    }
    Serial.println("connected");
  }
  else
  {
    if (!client.connected())
    {
      mqtt_connect();
    }
    else
    {
      client.loop();
    }
  }

  now = time(nullptr);
  float h = dht.readHumidity();
  float t = dht.readTemperature();
  float i = getBrigthness();
  //JSON para humedad
  String json = "{\"value\": "+ String(h) + "}";
  char payload1[json.length()+1];
  json.toCharArray(payload1,json.length()+1);
  //JSON para temperatura
  json = "{\"value\": "+ String(t) + "}";
  char payload2[json.length()+1];
  json.toCharArray(payload2,json.length()+1);
  //JSON para luminosidad
  json = "{\"value\": "+ String(i) + "}";
  char payload3[json.length()+1];
  json.toCharArray(payload3,json.length()+1);

  //Si los valores recolectados no son indefinidos, se envían a los tópicos correspondientes
  if ( !isnan(h) && !isnan(t) && !isnan(t) ) {
    //Publica en el tópico de la humedad
    client.publish(MQTT_PUB_TOPIC1, payload1, false);
    //Publica en el tópico de la temperatura
    client.publish(MQTT_PUB_TOPIC2, payload2, false);
    //Publica en el tópico de la luminosidad
    client.publish(MQTT_PUB_TOPIC3, payload3, false);
  }

  //Imprime en el monitor serial la información recolectada
  Serial.print(MQTT_PUB_TOPIC1);
  Serial.print(" -> ");
  Serial.println(payload1);
  Serial.print(MQTT_PUB_TOPIC2);
  Serial.print(" -> ");
  Serial.println(payload2);
  Serial.print(MQTT_PUB_TOPIC3);
  Serial.print(" -> ");
  Serial.println(payload3);
  /*Espera 5 segundos antes de volver a ejecutar la función loop*/
  delay(5000);
}

float getBrigthness() {
  int sensorInput = analogRead(LIGHT_SENSOR);
  
  // Cálculo respuesta del sensor
  float lectura = ((((ADC_MAX_INPUT - sensorInput) * 100) / ADC_MAX_INPUT) * 3.357) - 178.06;

  if (lectura < 40.00){
    lectura = 0.00;
  }

  if(lectura > 80.00 ) {
    lectura = 100.00;
  }
  
  return round(lectura * 100.0) / 100.0;
}