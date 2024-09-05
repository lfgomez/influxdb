/*
  Exemplo aprendizado de maquina usando kmeans no ESP8266. A parte de comunicacao eh baseada 
  HTTP usando como endpoint a API v2 de um banco de dados influxDB.
*/

//Inserindo os dados do termistor usado
        
// Resistencia nominal a 25C (Estamos utilizando um MF52 com resistencia nominal de 1kOhm)
#define TERMISTORNOMINAL 1000      
// Temperatura na qual eh feita a medida nominal (25C)
#define TEMPERATURANOMINAL 25   
//Quantas amostras usaremos para calcular a tensao media (um numero entre 4 e 10 eh apropriado)
#define AMOSTRAS 4
// Coeficiente Beta (da equacao de Steinhart-Hart) do termistor (segundo o datasheet eh 3100)
#define BETA 3100
// Valor da resistencia utilizada no divisor de tensao (para temperatura ambiente, qualquer resistencia entre 470 e 2k2 pode ser usada)
#define RESISTOR 470   

#define NUM_DATA 100
#define NUM_CLUSTERS 2
#define MAX_ITERATIONS 1000

#include <ESP8266WiFi.h>
#include <ArduinoHttpClient.h>
#include <ArduinoJson.h> 

typedef struct {
    double value;
} Data;

typedef struct {
    Data centroid;
    Data points[NUM_DATA];
    int num_points;
} Cluster;

// Prototypes
double distance(Data* a, Data* b);
void kmeans(Data* dataset, int num_data, Cluster* clusters, int num_clusters);

// Vamos primeiramente conectar o ESP8266 com a rede Wireless (mude os parâmetros abaixo para sua rede).

// Dados da rede WiFi
const char* ssid = "";
const char* password = "";

// Dados do servidor / sensor
const char* ID = "";
const char* bucket = "";
const char* token = "";
const char* user = "";
const char* influxdb_address = "64.227.106.209";


//Variaveis do termometro
float temperature;
float tensao;
float resistencia_termistor;
int i = 0;

//Variaveis do modelo
float threshold;
int cluster = 0;
int ans = 0;


//Criando os objetos de conexão com a rede e com o servidor rodando o influxDB.
WiFiClient espClient;
HttpClient client = HttpClient(espClient, influxdb_address, 8086);

int send_data(String ID, String bucket, String token, String user, float temperatura, int grupo) {
  String postData = ID+ " grupo=" + String(grupo) + ",temperatura=" +String(temperatura)+"\n";
  //Serial.println(postData);
  String api_string = "/api/v2/write?org="+user+"&bucket="+bucket+"&precision=ns";
  String auth_string = "Token "+token;
  client.beginRequest();
  client.post(api_string);
  client.sendHeader("Content-Type", "application/x-www-form-urlencoded");
  client.sendHeader("Content-Length", postData.length());
  client.sendHeader("Authorization", auth_string);
  client.beginBody();
  client.print(postData);
  client.endRequest();
  //Resposta
  int statusCode = client.responseStatusCode();
  return statusCode;
}

void setup_wifi() {
  delay(10);
  // Agora vamos nos conectar em uma rede Wifi
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    //Imprimindo pontos na tela ate a conexao ser estabelecida!
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi conectado");
  Serial.println("Endereco de IP: ");
  Serial.println(WiFi.localIP());
}

double distance(Data* a, Data* b) {
    return fabs(a->value - b->value);
}
//Funcao definindo o metodo kmeans
//Nota: voce consegue melhorar a inicializacao! Ela foi escrita para funcionar bem apenas com 2 classes.
void kmeans(Data* dataset, int num_data, Cluster* clusters, int num_clusters) {
    // Initialize clusters with first num_clusters datapoints
    for (int i = 0; i < num_clusters; i++) {
        clusters[i].centroid = dataset[i];
        clusters[i].num_points = 0;
    }
    clusters[1].centroid = dataset[int(NUM_DATA/2)];

    bool changes = true;
    int iterations = 0;
    
    while (changes && iterations < MAX_ITERATIONS) {
        changes = false;
        
        // Clear clusters
        for (int i = 0; i < num_clusters; i++) {
            clusters[i].num_points = 0;
        }

        // Assign data points to clusters
        for (int i = 0; i < num_data; i++) {
            int closest_cluster = 0;
            double min_distance = distance(&dataset[i], &clusters[0].centroid);
            for (int j = 1; j < num_clusters; j++) {
                double current_distance = distance(&dataset[i], &clusters[j].centroid);
                if (current_distance < min_distance) {
                    min_distance = current_distance;
                    closest_cluster = j;
                }
            }
            clusters[closest_cluster].points[clusters[closest_cluster].num_points] = dataset[i];
            clusters[closest_cluster].num_points++;
        }

        // Recalculate centroids
        for (int i = 0; i < num_clusters; i++) {
            Data new_centroid = {0};
            for (int j = 0; j < clusters[i].num_points; j++) {
                new_centroid.value += clusters[i].points[j].value;
            }
            new_centroid.value /= clusters[i].num_points;

            if (new_centroid.value != clusters[i].centroid.value) {
                changes = true;
                clusters[i].centroid = new_centroid;
            }
        }

        iterations++;
    }
}

void setup()
{
  //Configurando a porta Serial e escolhendo o servidor MQTT
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);
  Serial.begin(115200);
  //Encontrando o threshold para duas classes usando kmeans
  //Nota: voce consegue reescrever a funcao para mais classes passando o float para um vetor.
  threshold = trainning_kmeans();
  setup_wifi();
}

float trainning_kmeans(){
  Data dataset[NUM_DATA];
  for (int i = 0; i < NUM_DATA; i++) {
    dataset[i].value = get_temperature();
    delay(200);
  }
  Cluster clusters[NUM_CLUSTERS];

    kmeans(dataset, NUM_DATA, clusters, NUM_CLUSTERS);

    // Encontrando thresholds
    double thresholds[NUM_CLUSTERS-1];
    for (int i = 0; i < NUM_CLUSTERS - 1; i++) {
        thresholds[i] = (clusters[i].centroid.value + clusters[i+1].centroid.value) / 2;
    }

    // Imprimindo clusters
    for (int i = 0; i < NUM_CLUSTERS; i++) {
        Serial.print("Cluster "); 
        Serial.println(i);
        Serial.print("Centroid "); 
        Serial.println(clusters[i].centroid.value);
        for (int j = 0; j < clusters[i].num_points; j++) {
            Serial.println(clusters[i].points[j].value);
        }
    }

    // Imprimindo thresholds
    Serial.print("\nThreshold:\n");
    for (int i = 0; i < NUM_CLUSTERS - 1; i++) {
        Serial.print(thresholds[i]);
    }
    return thresholds[0];
}

//Funcao para calcular a temperatura baseada nos dados do termistor
float get_temperature(){
  tensao = 0;
  //Encontrando a media do valor lido no ADC
  for (i=0; i< AMOSTRAS; i++) {
   tensao += analogRead(0)/AMOSTRAS;
   delay(10);
  }
  //Calculando a resistencia do Termistor
  resistencia_termistor = RESISTOR*tensao/(1023-tensao);
  //Equacao de Steinhart-Hart
  temperature = (1 / (log(resistencia_termistor/TERMISTORNOMINAL) * 1/BETA + 1/(TEMPERATURANOMINAL + 273.15))) - 273.15;
  //Vamos imprimir via Serial o resultado para ajudar na verificacao
  Serial.print("Resistencia do Termistor: "); 
  Serial.println(resistencia_termistor);
  Serial.print("Temperatura: "); 
  Serial.println(temperature);
  return temperature;
}

void loop()
{
  //O programa em si eh muito simples: 
  //se nao estiver conectado no Broker MQTT, se conecte!
  temperature = get_temperature();
  //Calcule a temperatura e veja em que cluster ela se encontra
  if (temperature>threshold){
    cluster=1;
    digitalWrite(LED_BUILTIN, LOW);
  }
  else{
    digitalWrite(LED_BUILTIN, HIGH);
    cluster=0;
  }
  //Enviando via HTTP o resultado calculado da temperatura
  ans = send_data(ID, bucket, token, user, temperature, cluster);
  Serial.println("Resposta: " + String(ans) + "\n");
  
  //Gerando um delay de 2 segundos antes do loop recomecar
  delay(2000);
}