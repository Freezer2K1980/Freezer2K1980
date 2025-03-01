#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <time.h>
#include <ESPAsyncWebServer.h>

const char* ssid = "eolo casa";
const char* password = "carolyne13";

// Sostituisci con il tuo URL webhook di Discord
//const char* discord_webhook_url = "https://discord.com/api/webhooks/1292497259283349504/L0yoR92Jy9cCpji0hiFKRmxpULU71TyLDNDJmZke-Pyd68b0SaqG7ftq29np6ZTpKqSg";

#define BOT_TOKEN "7040107009:AAEYQ9CGNXuKnnOJOpLo6kYHyTjdBWalU6A"
#define SOURCE_GROUP_ID "-4586421275"
//#define DESTINATION_GROUP_ID "-4052460379"

void handleNewMessages(int numNewMessages);
void sendResponseToAll(const String& response);
void sendDiscordMessage(String content);
String trovaRisposta(const String& comando);
bool isMerce(const String& risposta);
bool isLuogo(const String& risposta);
bool isExtra(const String& risposta);
bool isCoordinata(const String& risposta);
String processMultipleCommands(const String& text);
void processTimeScheduleCommand(const String& text);
void executeSchedule();
void printLocalTime();
void updateNextScheduleTime();
const String LINGUE_SUPPORTATE[] = { "de", "fr", "ru", "es" };
const int MAX_LINGUE = 4;
String scheduledMerci[20];  // Array per 4 merci + 4 lingue
const char* epoche[] = { "Epoca 1", "Epoca 2", "Epoca 3", "Epoca 4", "Epoca 5", "Epoca 6" };
const char* categorie[] = { "Luoghi", "Coordinate", "Extra" };
const int comandiPerEpoca[] = { 10, 8, 8, 9, 8, 6 };
const int comandiPerCategoria[] = { 154, 18, 5 };
struct LocationRange {
    String name;
    int startIndex;
    int endIndex;
};

const LocationRange locations[] = {
  { "Camino", 49, 98 },    // Da altoguado a zuccaria
  { "Colosseo", 99, 152 }, // Da amburgo a porta
  { "Golden", 153, 202 }   // Da Albuquerque a Wichita
};

struct tm timeinfo;
String scheduledCommand = "";
bool isSingleExecution = false;
int scheduleIntervalMinutes = 0;  // Primo intervallo
int secondIntervalMinutes = 0;    // Secondo intervallo
int originalMinute = 0;           // Dichiarato una sola volta
bool isScheduleActive = false;
bool isPrimoSet = true;
bool firstExecution = true;

// Time configuration - correct Italian time
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 3600;   // Base UTC+1
const int daylightOffset_sec = 0;  // No additional offset

String formatTime(String timeStr) {
    int colonPos = timeStr.indexOf('.');
    if (colonPos != -1) {
        String hours = timeStr.substring(0, colonPos);
        String minutes = timeStr.substring(colonPos + 1);

        if (hours.length() == 1) {
            hours = "0" + hours;
        }

        if (minutes.length() == 1) {
            minutes = "0" + minutes;
        }

        return hours + ":" + minutes;
    }
    return timeStr;
}

void setupTime() {
    setenv("TZ", "CET-1", 1);
    tzset();
    configTime(3600, 0, ntpServer);  // Fixed offset for Italian time
}

void printLocalTime() {
    if (!getLocalTime(&timeinfo)) {
        return;
    }
    char timeStringBuff[25];
    strftime(timeStringBuff, sizeof(timeStringBuff), "%H:%M:%S", &timeinfo);
    Serial.print("Current time: ");
    Serial.println(timeStringBuff);
}

WiFiClientSecure client;
UniversalTelegramBot bot(BOT_TOKEN, client);

unsigned long lastTimeChecked = 0;
const unsigned long checkInterval = 1000;

struct Comando {
    const char* comando;
    const char* risposta;     // mantiene il formato esistente per retrocompatibilità
    const char* emoji;        // per Telegram
    const char* descrizione;  // per Telegram
};


// Array completo di tutti i comandi
const Comando comandi[] = {
  // Epoca 1
  { "/legno", ":wood: * Legno * Wood *" },
  { "/carbone", ":coal: * Carbone * Coal *" },
  { "/grano", ":grain: * Grano * Wheat *" },
  { "/mucche", ":cattle: * Bestiame * Cows *" },
  { "/tavole", ":boards: * Tavole * Boards *" },
  { "/mineraleferro", ":iron_ore: * Minerale di Ferro * Iron Ore *" },
  { "/ferro", ":iron: * Ferro * Iron *" },
  { "/cotone", ":cotton: * Cotone * Cotton *" },
  { "/filati", ":thread: * Filati * Thread *" },
  { "/pax", ":passengers: * Passeggeri * Passenger *" },

  // Epoca 2
  { "/cuoio", ":leather: * Cuoio * Leather *" },
  { "/farina", ":flour: * Farina * Flour *" },
  { "/carne", ":meat: * Carne * Meat *" },
  { "/tessuti", ":textiles: * Tessuti * Textiles *" },
  { "/carta", ":paper: * Carta * Paper *" },
  { "/ferramenta", ":hardware: * Ferramenta * Hardware *" },
  { "/pane", ":pastries: * Pane * Pastries *" },
  { "/mineralerame", ":copper_ore: * Minerale di Rame * Copper Ore *" },

  // Epoca 3
  { "/quarzo", ":quartz: * Quarzo * Quartz *" },
  { "/tondini", ":copper: * Rame * Copper *" },
  { "/scarpe", ":shoes: * Scarpe * Shoes *" },
  { "/acciaio", ":steel: * Acciaio * Steel *" },
  { "/scatole", ":packaging: * Imballaggio * Packaging *" },
  { "/vetro", ":glassware: * Vetro * Glass *" },
  { "/cavi", ":wires: * Cavi * Wires *" },
  { "/tubi", ":pipes: * Tubi * Pipes *" },

  // Epoca 4
  { "/finestre", ":windows: * Finestre * Windows *" },
  { "/petrolio", ":crude_oil: * Petrolio Greggio * Crude Oil *" },
  { "/panini", ":food: * Cibo * Food *" },
  { "/silicio", ":silicon: * Silicio * Silicon *" },
  { "/lampade", ":lamps: * Lampade * Lamps *" },
  { "/vestiti", ":clothing: * Vestiti * Clothing *" },
  { "/lamiere", ":sheet_metal: * Lamiera * Sheet Metal *" },
  { "/chimici", ":chemicals: * Prodotti Chimici * Chemicals *" },
  { "/inox", ":stainless_steel: * Acciaio Inox * Stainless Steel *" },

  // Epoca 5
  { "/motori", ":machinery: * Impianti * Machinery *" },
  { "/bauxite", ":bauxite: * Bauxite * Bauxite *" },
  { "/travi", ":steel_beams: * Travi d'Acciaio * Steel Beams *" },
  { "/benzina", ":petrol: * Benzina * Petrol *" },
  { "/plastica", ":plastics: * Palline * Plastics *" },
  { "/alluminio", ":aluminium: * Alluminio * Aluminium *" },
  { "/ceramica", ":pottery: * Ceramiche * Pottery *" },
  { "/auto", ":cars: * Automobili * Cars *" },

  // Epoca 6
  { "/chip", ":electronics: * Elettronica * Electronics *" },
  { "/pentole", ":household_supplies: * Casalinghi * Household *" },
  { "/papere", ":toys: * Giocattoli * Toys *" },
  { "/palloni", ":sports_goods: * Accessori Sport * Sports Goods *" },
  { "/vasche", ":toiletries: * Articoli da Bagno * Toiletries *" },
  { "/pastiglie", ":pharmaceuticals: * Medicinali * Pharma *" },

  // Luoghi
  //Camino
  { "/altoguado", "Altoguado" },
  { "/arianuova", "Arianuova" },
  { "/aspoli", "Aspoli" },
  { "/barivilla", "Barivilla" },
  { "/bellariva", "Bellariva" },
  { "/belvecchio", "Belvecchio" },
  { "/bluesi", "Bluesi" },
  { "/borgolargo", "Borgolargo" },
  { "/bosconero", "Bosconero" },
  { "/capovento", "Capovento" },
  { "/cerasia", "Cerasia" },
  { "/cervizia", "Cervizia" },
  { "/chianosso", "Chianosso" },
  { "/clainoro", "Clainoro" },
  { "/dantia", "Dantia" },
  { "/dragania", "Dragania" },
  { "/eporadia", "Eporadia" },
  { "/ferratia", "Ferratia" },
  { "/fiumebianco", "Fiumebianco" },
  { "/fortegrande", "Fortegrande" },
  { "/gognati", "Gognati" },
  { "/imperia", "Imperia" },
  { "/lucresia", "Lucresia" },
  { "/maltenga", "Maltenga" },
  { "/melabero", "Melabero" },
  { "/merturi", "Merturi" },
  { "/milasano", "Milasano" },
  { "/miralago", "Miralago" },
  { "/montargento", "Montargento" },
  { "/nagabria", "Nagabria" },
  { "/norandia", "Norandia" },
  { "/osteriano", "Osteriano" },
  { "/pietralancia", "Pietralancia" },
  { "/portanova", "Portanova" },
  { "/porto_snarri", "Porto Snarri" },
  { "/porto_loco", "Porto Loco" },
  { "/ramolungo", "Ramolungo" },
  { "/rianto", "Rianto" },
  { "/roccadoro", "Roccadoro" },
  { "/roccaferro", "Roccaferro" },
  { "/roccari", "Roccari" },
  { "/roccarubia", "Roccarubia" },
  { "/scatania", "Scatania" },
  { "/severiano", "Severiano" },
  { "/shardala", "Shardala" },
  { "/spertia", "Spertia" },
  { "/spircton", "Spircton" },
  { "/storiolo", "Storiolo" },
  { "/vinagria", "Vinagria" },
  { "/zuccaria", "Zuccaria" },
  
  // Colosseo
  { "/amburgo", "Amburgo" },
  { "/amsterdam", "Amsterdam" },
  { "/atene", "Atene" },
  { "/barcellona", "Barcellona" },
  { "/belgrado", "Belgrado" },
  { "/berlino", "Berlino" },
  { "/bordeaux", "Bordeaux" },
  { "/bruxelles", "Bruxelles" },
  { "/bucarest", "Bucarest" },
  { "/budapest", "Budapest" },
  { "/cardiff", "Cardiff" },
  { "/chisinau", "Chisinau" },
  { "/cologne", "Cologne" },
  { "/copenaghen", "Copenaghen" },
  { "/donetsk", "Donetsk" },
  { "/dublino", "Dublino" },
  { "/gdansk", "Gdansk" },
  { "/glasgow", "Glasgow" },
  { "/gothenburg", "Gothenburg" },
  { "/helsinki", "Helsinki" },
  { "/istanbul", "Istanbul" },
  { "/kiev", "Kiev" },
  { "/lione", "Lione" },
  { "/lisbona", "Lisbona" },
  { "/londra", "Londra" },
  { "/madrid", "Madrid" },
  { "/malaga", "Malaga" },
  { "/manchester", "Manchester" },
  { "/marsiglia", "Marsiglia" },
  { "/milano", "Milano" },
  { "/minsk", "Minsk" },
  { "/monaco", "Monaco" },
  { "/mosca", "Mosca" },
  { "/napoli", "Napoli" },
  { "/oslo", "Oslo" },
  { "/palermo", "Palermo" },
  { "/parigi", "Parigi" },
  { "/porto", "Porto" },
  { "/praga", "Praga" },
  { "/riga", "Riga" },
  { "/roma", "Roma" },
  { "/sanpeter", "San Pietroburgo" },
  { "/sarajevo", "Sarajevo" },
  { "/sofia", "Sofia" },
  { "/stoccolma", "Stoccolma" },
  { "/varsavia", "Varsavia" },
  { "/vienna", "Vienna" },
  { "/vilnius", "Vilnius" },
  { "/zagabria", "Zagabria" },
  { "/zurigo", "Zurigo" },
  { "/acropoli", "Acropoli Atene" },
  { "/cattedrale", "Cattedrale Sofia" },
  { "/moschea", "Moschea Istanbul" },
  { "/porta", "Porta Brandeburgo" },
  
  //Golden 
  { "/albu", "Albuquerque" },
  { "/amarillo", "Amarillo" },
  { "/augusta", "Augusta" },
  { "/bismarck", "Bismarck" },
  { "/boise", "Boise" },
  { "/boston", "Boston" },
  { "/buffalo", "Buffalo" },
  { "/casper", "Casper" },
  { "/charlotte", "Charlotte" },
  { "/chicago", "Chicago" },
  { "/columbus", "Columbus" },
  { "/dallas", "Dallas" },
  { "/davenport", "Davenport" },
  { "/denver", "Denver" },
  { "/detroit", "Detroit" },
  { "/elpaso", "El Paso" },
  { "/eugene", "Eugene" },
  { "/helena", "Helena" },
  { "/houston", "Houston" },
  { "/indi", "Indianapolis" },
  { "/jack", "Jacksonville" },
  { "/kansas", "Kansas City" },
  { "/lasvegas", "Las Vegas" },
  { "/littlerock", "Little Rock" },
  { "/la", "Los Angeles" },
  { "/memphis", "Memphis" },
  { "/miami", "Miami" },
  { "/midland", "Midland" },
  { "/milwaukee", "Milwaukee" },
  { "/minneapolis", "Minneapolis" },
  { "/montgomery", "Montgomery" },
  { "/nashville", "Nashville" },
  { "/neworleans", "New Orleans" },
  { "/ny", "New York" },
  { "/norfolk", "Norfolk" },
  { "/okla", "Oklahoma City" },
  { "/omaha", "Omaha" },
  { "/phoenix", "Phoenix" },
  { "/portland", "Portland" },
  { "/rapid", "Rapid City" },
  { "/reno", "Reno" },
  { "/slc", "Salt Lake City" },
  { "/sanantonio", "San Antonio" },
  { "/sandiego", "San Diego" },
  { "/sanfrancisco", "San Francisco" },
  { "/seattle", "Seattle" },
  { "/stlouis", "St. Louis" },
  { "/walla", "Walla Walla" },
  { "/washington", "Washington" },
  { "/wichita", "Wichita" },

  // Coordinate
  { "/e", "* Est * East *" },
  { "/e2", "* Est * East II *" },
  { "/o", "* Ovest * West *" },
  { "/o2", "* Ovest * West II *" },
  { "/n", "* Nord * North *" },
  { "/n2", "* Nord * North II *" },
  { "/s", "* Sud * South *" },
  { "/s2", "* Sud * South II *" },
  { "/ne", "* Nord-Est * North-East *" },
  { "/ne2", "* Nord-Est * North-East II *" },
  { "/se", "* Sud-Est * South-East *" },
  { "/se2", "* Sud-Est * South-East II *" },
  { "/so", "* Sud-Ovest * South-West *" },
  { "/so2", "* Sud-Ovest * South-West II *" },
  { "/no", "* Nord-Ovest * North-West *" },
  { "/no2", "* Nord-Ovest * North-West II *" },
  { "/mag", "* Magazzino * Warehouse*" },
  { "/har", "* Porto * Harbour *" },

  // Extra
  { "/cp", "*Con potenziamenti * With upgrades" },
  { "/dc", "*Da dove conviene * Best time" },
  { "/ricalcolo", "* Aspettare ricalcolo * Wait recalculation *" },
  { "/city", "* Parcheggiare i treni in città * Park trains on city *" },
  { "/imm", "*Immediate*" }
};

// Array traduzioni Tedesco
const char* traduzioniTedesco[] = {
  // Epoca 1
  "Holz", "Kohle", "Weizen", "Vieh", "Bretter", "Eisenerz", "Eisen", "Baumwolle", "Garn", "Passagiere",
  // Epoca 2
  "Leder", "Mehl", "Fleisch", "Textilien", "Papier", "Werkzeug", "Gebäck", "Kupfererz",
  // Epoca 3
  "Quarz", "Kupfer", "Schuhe", "Stahl", "Verpackung", "Glas", "Kabel", "Rohre",
  // Epoca 4
  "Fenster", "Rohöl", "Nahrung", "Silizium", "Lampen", "Kleidung", "Blech", "Chemikalien", "Edelstahl",
  // Epoca 5
  "Maschinen", "Bauxit", "Stahlträger", "Benzin", "Kunststoffe", "Aluminium", "Keramik", "Autos",
  // Epoca 6
  "Elektronik", "Haushaltswaren", "Spielzeug", "Sportartikel", "Toilettenartikel", "Pharmazeutika"
};

// Array traduzioni Spagnolo
const char* traduzioniSpagnolo[] = {
  // Epoca 1
  "Madera", "Carbón", "Trigo", "Ganado", "Tablones", "Min. Hierro", "Hierro", "Algodón", "Hilo", "Pasajeros",
  // Epoca 2
  "Cuero", "Harina", "Carne", "Telas", "Papel", "Herramientas", "Bollería", "Min. Cobre",
  // Epoca 3
  "Cuarzo", "Cobre", "Calzado", "Acero", "Embalajes", "Cristal", "Cables", "Tuberías",
  // Epoca 4
  "Ventanas", "Crudo", "Comestibles", "Sílice", "Bombillas", "Ropa", "Planchas Metálicas", "Productos Químicos", "Acero Inoxidable",
  // Epoca 5
  "Maquinaria", "Bauxita", "Vigas de Acero", "Petróleo", "Plásticos", "Aluminio", "Cerámica", "Automóviles",
  // Epoca 6
  "Aparatos Electrónicos", "Artículos del Hogar", "Juguetes", "Material Deportivo", "Artículos de Aseo", "Productos Farmacéuticos"
};

// Array traduzioni Francese
const char* traduzioniFrancese[] = {
  // Epoca 1
  "Bois", "Charbon", "Blé", "Bétail", "Planches", "Min. de Fer", "Fer", "Coton", "Fil", "Passagers",
  // Epoca 2
  "Cuir", "Farine", "Viande", "Textiles", "Papier", "Outils", "Pâtisseries", "Min. de Cuivre",
  // Epoca 3
  "Quartz", "Cuivre", "Chaussures", "Acier", "Emballage", "Verre", "Câbles", "Tuyaux",
  // Epoca 4
  "Fenêtres", "Pétrole Brut", "Nourriture", "Silicium", "Lampes", "Vêtements", "Tôle", "Produits Chimiques", "Acier Inoxydable",
  // Epoca 5
  "Machines", "Bauxite", "Poutres d'Acier", "Essence", "Plastiques", "Aluminium", "Céramique", "Voitures",
  // Epoca 6
  "Électronique", "Articles Ménagers", "Jouets", "Articles de Sport", "Articles de Toilette", "Produits Pharmaceutiques"
};

// Array traduzioni Russo
const char* traduzioniRusso[] = {
  // Epoca 1
  "Дерево", "Уголь", "Пшеница", "Скот", "Доски", "Железная руда", "Железо", "Хлопок", "Нить", "Пассажиры",
  // Epoca 2
  "Кожа", "Мука", "Мясо", "Ткани", "Бумага", "Инструменты", "Выпечка", "Медная руда",
  // Epoca 3
  "Кварц", "Медь", "Обувь", "Сталь", "Упаковка", "Стекло", "Кабели", "Трубы",
  // Epoca 4
  "Окна", "Нефть", "Еда", "Кремний", "Лампы", "Одежда", "Листовой металл", "Химикаты", "Нержавеющая сталь",
  // Epoca 5
  "Механизмы", "Боксит", "Стальные балки", "Бензин", "Пластмассы", "Алюминий", "Керамика", "Автомобили",
  // Epoca 6
  "Электроника", "Хозтовары", "Игрушки", "Спортивные товары", "Туалетные принадлежности", "Медикаменты"
};

const int numComandi = sizeof(comandi) / sizeof(comandi[0]);

String generateCommandList(String location = "") {
  bool isLocationSpecific = !location.isEmpty();
  String logPrefix = isLocationSpecific ? "per " + location : "generale";

  Serial.println("\n=== Inizio generazione lista comandi " + logPrefix + " ===");
  String commandList = "Elenco dei comandi disponibili:\n\n";
  int comandoIndex = 0;

  // Generazione sezione Epoche
  Serial.println("Generazione sezione epoche storiche...");
  for (int e = 0; e < 6; e++) {
    Serial.println("-> Elaborazione " + String(epoche[e]));
    commandList += String(epoche[e]) + ":\n";
    for (int i = 0; i < comandiPerEpoca[e]; i++) {
      String comando = comandi[comandoIndex].comando;
      if (!comando.startsWith("/")) comando = "/" + comando;
      commandList += comando + " - " + String(comandi[comandoIndex].risposta) + "\n";
      comandoIndex++;
    }
    commandList += "\n";
  }

  if (isLocationSpecific) {
    // Generazione sezione Luoghi per location specifica
    Serial.println("\nGenerazione sezione luoghi specifici...");
    commandList += "Luoghi:\n\n";
    int inizioLuoghi = comandoIndex;
    int fineLuoghi = inizioLuoghi + comandiPerCategoria[0];

    for (int i = inizioLuoghi; i < fineLuoghi; i++) {
      String comando = comandi[i].comando;
      if (!comando.startsWith("/")) comando = "/" + comando;
      commandList += comando + " - " + location + "\n";
    }
    comandoIndex = fineLuoghi;
    commandList += "\n";
  }

  // Generazione sezione Categorie/Extra
  Serial.println("\nGenerazione sezioni " + String(isLocationSpecific ? "coordinate ed extra" : "categorie") + "...");
  for (int c = (isLocationSpecific ? 1 : 0); c < 3; c++) {
    Serial.println("-> Elaborazione categoria: " + String(categorie[c]));
    commandList += String(categorie[c]) + ":\n\n";
    for (int i = 0; i < comandiPerCategoria[c]; i++) {
      String comando = comandi[comandoIndex].comando;
      if (!comando.startsWith("/")) comando = "/" + comando;
      commandList += comando + " - " + String(comandi[comandoIndex].risposta) + "\n";
      comandoIndex++;
    }
    commandList += "\n";
  }

  Serial.println("\nGenerazione completata con successo!");
  Serial.println("Totale comandi elaborati: " + String(comandoIndex));
  Serial.println("=== Fine generazione lista " + logPrefix + " ===\n");

  return commandList;
}

// Variabili per il time schedule
String scheduledLuogo = "";
int scheduledHour = -1;
int scheduledMinute = -1;

void parseCommand(String command) {
    int lastSpace = command.lastIndexOf(' ');
    String lingua = command.substring(lastSpace + 1);
    
    // Verifica stato prima di processare
    if (!isScheduleActive) {
        return;
    }

    bool isLingua = false;
    for (int i = 0; i < MAX_LINGUE; i++) {
        if (lingua == LINGUE_SUPPORTATE[i]) {
            scheduledMerci[4] = lingua;
            isLingua = true;
            break;
        }
    }
}

void handleStopCommand() {
    if (!isScheduleActive) {
        return;
    }
    
    isScheduleActive = false;
    bot.sendMessage(SOURCE_GROUP_ID, "Programmazione oraria fermata.", "");
    Serial.println("Comando STOP ricevuto - Programmazione disattivata");
    
    // Reset ordinato degli stati
    isSingleExecution = false;
    isPrimoSet = true;
    firstExecution = true;
}

// Aggiungi il server web
AsyncWebServer server(80);

void setup() {
    Serial.begin(115200);
    delay(2000);  // Aggiungiamo un delay iniziale
    Serial.println("Avvio sistema...");

    // Inizializzazione stati
    isScheduleActive = false;
    isSingleExecution = false;
    isPrimoSet = true;
    firstExecution = true;

    // Connessione WiFi
    WiFi.mode(WIFI_STA);  // Impostiamo esplicitamente la modalità
    WiFi.begin(ssid, password);
    Serial.print("Connessione WiFi");
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }

    Serial.println("\nConnesso al WiFi");
    Serial.print("Indirizzo IP ESP32: ");
    Serial.println(WiFi.localIP());

    setupTime();

    client.setCACert(TELEGRAM_CERTIFICATE_ROOT); // Aggiungiamo il certificato
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

    int retries = 0;
    while (!getLocalTime(&timeinfo) && retries < 10) {
        Serial.println("Attesa sincronizzazione NTP...");
        delay(1000);
        retries++;
    }

    if (retries >= 10) {
        Serial.println("Errore sincronizzazione NTP. Riavviare il dispositivo.");
    }
    else {
        Serial.println("Ora sincronizzata correttamente!");
        printLocalTime();
    }

    Serial.println("Bot avviato. In attesa di messaggi...");

    // Configura il server web
    server.on("/", HTTP_GET, [](AsyncWebServerRequest* request) {
        request->send(200, "text/html", "<h1>Interfaccia Web per il Bot</h1><form action=\"/command\" method=\"POST\"><input type=\"text\" name=\"command\" placeholder=\"Inserisci comando\"><input type=\"submit\" value=\"Invia\"></form>");
        });

    server.on("/command", HTTP_POST, [](AsyncWebServerRequest* request) {
        String command;
        if (request->hasParam("command", true)) {
            command = request->getParam("command", true)->value();
            Serial.println("Comando ricevuto: " + command);
            // Processa il comando ricevuto
            String response = processMessage(command);
            request->send(200, "text/plain", "Comando ricevuto: " + command + "\nRisposta: " + response);
        }
        else {
            request->send(400, "text/plain", "Parametro 'command' mancante");
        }
        });

    server.begin();
}

bool isTimeValid() {
    if (!getLocalTime(&timeinfo)) {
        return false;
    }
    return timeinfo.tm_year + 1900 >= 2010;
}

void checkSchedule() {
    if (!isScheduleActive) return;

    if (timeinfo.tm_hour == scheduledHour && timeinfo.tm_min == scheduledMinute) {
        // Execute scheduled command
        if (firstExecution || (timeinfo.tm_min == originalMinute)) {
            executeScheduledCommand();
        }
    }
}

void executeScheduledCommand() {
    // Your existing command execution code
    bot.sendMessage(SOURCE_GROUP_ID, scheduledCommand, "");
    Serial.println("Comando schedulato eseguito: " + scheduledCommand);

    // Update for next execution
    if (!isSingleExecution) {
        if (firstExecution) {
            scheduledMinute = (originalMinute + scheduleIntervalMinutes) % 60;
            scheduledHour = (scheduledHour + (originalMinute + scheduleIntervalMinutes) / 60) % 24;
            firstExecution = false;
        }
        else {
            scheduledMinute = (scheduledMinute + 60) % 60;
            scheduledHour = (scheduledHour + 1) % 24;
        }
    }
    else {
        isScheduleActive = false;
    }
}

void loop() {
    static unsigned long lastNTPSync = 0;
    const unsigned long NTP_SYNC_INTERVAL = 86400000; // 24 ore in millisecondi

    // Sincronizzazione NTP giornaliera
    if (millis() - lastNTPSync >= NTP_SYNC_INTERVAL) {
        Serial.println("Avvio sincronizzazione NTP giornaliera...");
        configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
        setenv("TZ", "CET-1CEST,M3.5.0,M10.5.0/3", 1);
        tzset();
        lastNTPSync = millis();
        Serial.println("Sincronizzazione NTP completata");
    }

    // Verifica validità tempo
    if (!isTimeValid()) {
        Serial.println("Tempo non valido, avvio risincronizzazione...");
        configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
        delay(1000);
        return;
    }

    // Controllo nuovi messaggi
    if (millis() - lastTimeChecked > checkInterval) {
        int numNewMessages = bot.getUpdates(bot.last_message_received + 1);

        if (numNewMessages > 0) {
            Serial.println("Elaborazione " + String(numNewMessages) + " nuovi messaggi");

            for (int i = 0; i < numNewMessages; i++) {
                String chatId = String(bot.messages[i].chat_id);
                String text = bot.messages[i].text;
                Serial.println("Messaggio ricevuto: " + text);

                if (text.equalsIgnoreCase("lista comandi") || text == "/list") {
                    Serial.println("Richiesta lista comandi generale");
                    String lista = generateCommandList();
                    if (bot.sendMessage(chatId, lista, "")) {
                        Serial.println("Lista comandi generale inviata con successo");
                    }
                    else {
                        Serial.println("Errore nell'invio della lista comandi generale");
                        bot.sendMessage(chatId, "Errore nell'invio della lista comandi", "");
                    }
                }
                else if (text == "/lista_camino" || text.equalsIgnoreCase("lista camino")) {
                    Serial.println("Richiesta lista comandi Camino");
                    sendLocationCommandList(chatId, "Camino");
                }
                else if (text == "/lista_colosseo" || text.equalsIgnoreCase("lista colosseo")) {
                    Serial.println("Richiesta lista comandi Colosseo");
                    sendLocationCommandList(chatId, "Colosseo");
                }
                else if (text == "/lista_golden" || text.equalsIgnoreCase("lista golden")) {
                    Serial.println("Richiesta lista comandi Golden");
                    sendLocationCommandList(chatId, "Golden");
                }
                else {
                    Serial.println("Elaborazione altri comandi ricevuti");
                    handleNewMessages(numNewMessages);
                }
            }
        }
        lastTimeChecked = millis();
    }

    // Gestione schedule
    if (isScheduleActive) {
        if (getLocalTime(&timeinfo)) {
            if (timeinfo.tm_hour == scheduledHour && timeinfo.tm_min == scheduledMinute) {
                Serial.println("Esecuzione operazioni programmate");
                executeSchedule();
            }
        }
    }
}

void sendLocationCommandList(String chatId, String location) {
    Serial.println("\n=== Inizio generazione lista comandi per " + location + " ===");

    int startIndex = 0;
    int endIndex = 0;
    int comandoIndex = 0;
    String mainList = "Comandi principali " + location + ":\n\n";

    // Trova gli indici corretti per la location
    for (const LocationRange& loc : locations) {
        if (loc.name == location) {
            startIndex = loc.startIndex;
            endIndex = loc.endIndex;
            break;
        }
    }

    // Sezione Epoche
    for (int e = 0; e < 6; e++) {
        Serial.println("Elaborazione epoca " + String(e + 1));
        mainList += String(epoche[e]) + ":\n";
        for (int i = 0; i < comandiPerEpoca[e]; i++) {
            mainList += String(comandi[comandoIndex].comando) + " - " + String(comandi[comandoIndex].risposta) + "\n";
            comandoIndex++;
        }
        mainList += "\n";
    }

    comandoIndex += comandiPerCategoria[0];

    // Sezione Coordinate ed Extra
    for (int c = 1; c < 3; c++) {
        Serial.println("Elaborazione categoria " + String(categorie[c]));
        mainList += String(categorie[c]) + ":\n\n";
        for (int i = 0; i < comandiPerCategoria[c]; i++) {
            mainList += String(comandi[comandoIndex].comando) + " - " + String(comandi[comandoIndex].risposta) + "\n";
            comandoIndex++;
        }
        mainList += "\n";
    }

    bot.sendMessage(chatId, mainList, "");

    // Lista luoghi specifica per la location
    String placesList = "Luoghi di " + location + ":\n\n";
    for (int i = startIndex; i <= endIndex; i++) {
        placesList += String(comandi[i].comando) + " - " + String(comandi[i].risposta) + "\n";
    }

    bot.sendMessage(chatId, placesList, "");
}

String processMessage(String message) {
    Serial.println("\n=== INIZIO ELABORAZIONE NUOVO MESSAGGIO ===");
    Serial.print("Timestamp: ");
    Serial.println(millis());

    message.trim();
    Serial.println("Messaggio originale (pre-trim): '" + message + "'");
    Serial.print("Lunghezza messaggio: ");
    Serial.println(message.length());

    String words[10];
    int wordCount = 0;
    int start = 0;
    int end;

    Serial.println("\n--- FASE 1: SEPARAZIONE PAROLE ---");
    Serial.println("Inizio scansione caratteri...");

    while (start < message.length()) {
        end = message.indexOf(' ', start);
        if (end == -1) {
            end = message.length();
            Serial.println("Raggiunta fine messaggio");
        }

        String word = message.substring(start, end);
        word.trim();

        Serial.print("Parola estratta: '");
        Serial.print(word);
        Serial.println("'");
        Serial.print("Posizione: start=");
        Serial.print(start);
        Serial.print(", end=");
        Serial.println(end);

        if (word == "es" || word == "de" || word == "fr" || word == "ru") {
            word = "/" + word;
            Serial.println("→ Comando lingua identificato: " + word);
            Serial.println("→ Slash aggiunto automaticamente");
        }
        else if (!word.startsWith("/")) {
            word = "/" + word;
            Serial.println("→ Parola normale: slash aggiunto");
            Serial.println("→ Risultato: " + word);
        }
        else {
            Serial.println("→ Parola già con slash: mantenuta invariata");
        }

        words[wordCount] = word;
        Serial.print("Parola memorizzata in posizione ");
        Serial.print(wordCount);
        Serial.print(": '");
        Serial.print(words[wordCount]);
        Serial.println("'");

        wordCount++;
        start = end + 1;
    }

    Serial.println("\n--- FASE 2: RICOSTRUZIONE MESSAGGIO ---");
    Serial.print("Numero totale parole elaborate: ");
    Serial.println(wordCount);

    String result = "";
    for (int i = 0; i < wordCount; i++) {
        result += words[i];
        if (i < wordCount - 1) {
            result += " ";
            Serial.print("Aggiunto spazio dopo parola ");
            Serial.println(i);
        }
        Serial.print("Costruzione parziale: '");
        Serial.print(result);
        Serial.println("'");
    }

    Serial.println("\n--- RISULTATO FINALE ---");
    Serial.println("Messaggio elaborato: '" + result + "'");
    Serial.print("Lunghezza finale: ");
    Serial.println(result.length());
    Serial.println("=== FINE ELABORAZIONE MESSAGGIO ===\n");

    return result;
}

void handleNewMessages(int numNewMessages) {
  Serial.println("\n=== Inizio gestione nuovi messaggi ===");
  Serial.println("Numero messaggi da elaborare: " + String(numNewMessages));

  for (int i = 0; i < numNewMessages; i++) {
    String chat_id = bot.messages[i].chat_id;
    String text = bot.messages[i].text;

    Serial.println("\nAnalisi messaggio " + String(i + 1) + "/" + String(numNewMessages));
    Serial.println("ID Chat: " + chat_id);
    Serial.println("Testo ricevuto: " + text);

    if (chat_id == String(SOURCE_GROUP_ID)) {
      Serial.println("Chat ID valido, analisi tipo comando...");

      if (text.indexOf(":") != -1) {
        Serial.println("Rilevato comando programmazione oraria");
        processTimeScheduleCommand(text);
      } else if (text.equalsIgnoreCase("stop")) {
        isScheduleActive = false;
        bot.sendMessage(SOURCE_GROUP_ID, "Programmazione oraria fermata.", "");
        Serial.println("Comando STOP ricevuto - Programmazione disattivata");
      } else if (text.equalsIgnoreCase("lista comandi") || text.equalsIgnoreCase("/list")) {
        Serial.println("Richiesta lista comandi");
        String commandList = generateCommandList();
        bot.sendMessage(SOURCE_GROUP_ID, commandList, "");
        Serial.println("Lista comandi inviata al gruppo");
      } else {
        Serial.println("\nElaborazione comando standard:");
        String processedText = processMessage(text);
        Serial.println("Testo dopo elaborazione: " + processedText);

        String response = processMultipleCommands(processedText);
        Serial.println("Risposta generata: " + response);

        if (response != "" && response != "Combinazione di comandi non valida o incompleta") {
          sendResponseToAll(response);
          isScheduleActive = false;
          Serial.println("Risposta inviata con successo a tutti i canali");
        } else {
          Serial.println("Comando non valido o incompleto - Nessuna risposta inviata");
        }
      }
    } else {
      Serial.println("Messaggio ignorato - ID Chat non autorizzato");
    }
  }

  Serial.println("=== Fine gestione nuovi messaggi ===\n");
}

void sendResponseToAll(const String& response) {
  // Invia al gruppo Telegram di origine
  bot.sendMessage(SOURCE_GROUP_ID, response, "");

  // Invia a Discord solo se la risposta non è un messaggio di errore
  if (response != "Combinazione di comandi non valida o incompleta" && response != "Comando non riconosciuto") {
    sendDiscordMessage(response);
    //bot.sendMessage(DESTINATION_GROUP_ID, response, "");
    Serial.println("Risposta inviata a tutti i canali: " + response);
  } else {
    Serial.println("Risposta inviata solo a Telegram: " + response);
  }
}

void sendDiscordMessage(String content) {
  HTTPClient http;

  //http.begin(discord_webhook_url);
  http.addHeader("Content-Type", "application/json");

  DynamicJsonDocument doc(1024);
  doc["content"] = content;
  String json;
  serializeJson(doc, json);

  int httpResponseCode = http.POST(json);
  if (httpResponseCode > 0) {
    Serial.print("Messaggio inviato a Discord. Codice HTTP: ");
    Serial.println(httpResponseCode);
  } else {
    Serial.print("Errore nell'invio del messaggio a Discord. Codice errore: ");
    Serial.println(httpResponseCode);
  }

  http.end();
}

String trovaRisposta(const String& comando) {
  String inputCommand = comando;
  String actualCommand;
  std::vector<String> languageCommands;

  Serial.println("\n--- Inizio elaborazione comando ---");
  Serial.println("Comando ricevuto: " + inputCommand);

  // Gestione comando senza slash
  if (!inputCommand.startsWith("/")) {
    inputCommand = "/" + inputCommand;
    Serial.println("Aggiunto slash al comando: " + inputCommand);
  }

  // Estrai tutti i comandi lingua
  while (inputCommand.startsWith("/")) {
    int spaceIndex = inputCommand.indexOf(" ");
    if (spaceIndex == -1) {
      actualCommand = inputCommand;
      Serial.println("Nessun altro comando lingua trovato, comando finale: " + actualCommand);
      break;
    }

    String prefix = inputCommand.substring(0, spaceIndex);
    Serial.println("Prefisso trovato: " + prefix);

    if (prefix == "/de" || prefix == "/es" || prefix == "/fr" || prefix == "/ru") {
      languageCommands.push_back(prefix);
      Serial.println("Comando lingua aggiunto: " + prefix);
      inputCommand = inputCommand.substring(spaceIndex + 1);
      Serial.println("Comando rimanente: " + inputCommand);
    } else {
      actualCommand = inputCommand;
      Serial.println("Comando finale trovato: " + actualCommand);
      break;
    }
  }

  Serial.println("Ricerca risposta per comando: " + actualCommand);

  // Trova la risposta base
  for (int i = 0; i < numComandi; i++) {
    if (String(comandi[i].comando) == actualCommand) {
      String risposta = comandi[i].risposta;
      Serial.println("Risposta base trovata: " + risposta);

      // Aggiungi le traduzioni per ogni lingua richiesta
      for (const String& lang : languageCommands) {
        Serial.println("Aggiunta traduzione per: " + lang);
        if (lang == "/de") {
          risposta += " * " + String(traduzioniTedesco[i]) + " *";
          Serial.println("Aggiunta traduzione tedesca");
        }
        if (lang == "/es") {
          risposta += " * " + String(traduzioniSpagnolo[i]) + " *";
          Serial.println("Aggiunta traduzione spagnola");
        }
        if (lang == "/fr") {
          risposta += " * " + String(traduzioniFrancese[i]) + " *";
          Serial.println("Aggiunta traduzione francese");
        }
        if (lang == "/ru") {
          risposta += " * " + String(traduzioniRusso[i]) + " *";
          Serial.println("Aggiunta traduzione russa");
        }
      }

      Serial.println("Risposta finale: " + risposta);
      Serial.println("--- Fine elaborazione comando ---\n");
      return risposta;
    }
  }

  Serial.println("Comando non riconosciuto");
  Serial.println("--- Fine elaborazione comando ---\n");
  return "Comando non riconosciuto";
}

bool isMerce(const String& risposta) {
  // Verifica se la risposta contiene un'emoji (inizia con :) e contiene asterischi
  return (risposta.startsWith(":") && risposta.indexOf(" * ") != -1 && risposta.endsWith(" *"));
}

bool isLuogo(const String& risposta) {
  return !risposta.startsWith("*") && !risposta.endsWith("*");
}

bool isExtra(const String& risposta) {
  return risposta.startsWith("*") && risposta.endsWith("*") && !isCoordinata(risposta) && !isMerce(risposta);
}

bool isCoordinata(const String& risposta) {
  return risposta.startsWith("*") && risposta.endsWith("*") && (risposta.indexOf("Nord") != -1 || risposta.indexOf("Sud") != -1 || risposta.indexOf("Est") != -1 || risposta.indexOf("Ovest") != -1);
}

int getIndiceComando(const String& risposta) {
  for (int i = 0; i < numComandi; i++) {
    if (String(comandi[i].risposta) == risposta) {
      return i;
    }
  }
  return -1;
}

String processMultipleCommands(const String& text) {
  // Rimuovi lo slash iniziale singolo se presente
  String processedText = text;
  if (processedText.startsWith("/ ")) {
    processedText = processedText.substring(2);
    Serial.println("Rimosso slash iniziale. Nuovo testo: " + processedText);
  }

  String merce1 = "";
  String merce2 = "";
  String luogo = "";
  String coordinata = "";
  String extra = "";
  std::vector<String> languageCommands;
  String baseResponse = "";

  Serial.println("Elaborazione comando: " + processedText);

  int start = 0, end;
  while (start < processedText.length()) {
    end = processedText.indexOf(' ', start);
    if (end == -1) end = processedText.length();
    String word = processedText.substring(start, end);
    word.trim();

    Serial.println("Parola originale: " + word);

    // Verifica se è un comando lingua
    if (word == "/de" || word == "/fr" || word == "/ru" || word == "/es") {
      languageCommands.push_back(word);
      Serial.println("Comando lingua trovato: " + word);
      start = end + 1;
      continue;
    }

    // Gestione emoji diretta
    if (word.startsWith(":") && word.endsWith(":")) {
      Serial.println("Rilevata emoji: " + word);
      for (int i = 0; i < numComandi; i++) {
        if (String(comandi[i].risposta).startsWith(word)) {
          word = String(comandi[i].comando);
          Serial.println("Emoji convertita in comando: " + word);
          break;
        }
      }
    }

    // Trova risposta per il comando
    String risposta = trovaRisposta(word);
    Serial.println("Parola: '" + word + "', Risposta: '" + risposta + "'");

    if (risposta != "Comando non riconosciuto") {
      if (isCoordinata(risposta)) {
        coordinata = risposta;
        Serial.println("Coordinata trovata: " + coordinata);
      } else if (isMerce(risposta)) {
        if (merce1 == "") {
          merce1 = risposta;
          Serial.println("Merce1 trovata: " + merce1);
        } else if (merce2 == "") {
          merce2 = risposta;
          Serial.println("Merce2 trovata: " + merce2);
        }
      } else if (isLuogo(risposta)) {
        luogo = risposta;
        Serial.println("Luogo trovato: " + luogo);
      } else if (isExtra(risposta)) {
        extra = risposta;
        Serial.println("Extra trovato: " + extra);
      }
    }
    start = end + 1;
  }

  Serial.println("Valori finali: merce1='" + merce1 + "', merce2='" + merce2 + "', luogo='" + luogo + "', coordinata='" + coordinata + "', extra='" + extra + "'");

  // Costruzione risposta base
  if (merce1 != "" && merce2 == "" && luogo != "" && coordinata != "") {
    String luogoUpper = luogo;
    luogoUpper.toUpperCase();
    baseResponse = merce1 + "\n\n" + luogoUpper + " " + coordinata;
    if (extra != "") {
      baseResponse += "\n\n" + extra;
    }
    Serial.println("Caso 1 attivato");
  } else if (merce1 != "" && luogo != "") {
    String luogoUpper = luogo;
    luogoUpper.toUpperCase();
    baseResponse = luogoUpper + "\n\n" + merce1;
    if (merce2 != "") {
      baseResponse += "\n\n" + merce2;
    }
    Serial.println("Caso 2 attivato");
  } else if (extra != "") {
    baseResponse = extra;
    Serial.println("Caso 3 attivato (solo extra)");
  } else {
    baseResponse = "Combinazione di comandi non valida o incompleta";
    Serial.println("Nessun caso attivato");
  }

  // Costruisci la risposta finale con le traduzioni
  String finalOutput = baseResponse;
  if (!languageCommands.empty()) {
    Serial.println("Inizio aggiunta traduzioni");

    auto aggiungiTraduzioni = [&](String merce) -> String {
      if (merce == "") return "";  // Se la merce è vuota, restituisci una stringa vuota
      String merceConLingue = merce;
      for (const String& lang : languageCommands) {
        if (lang == "/es") {
          merceConLingue += " * " + String(traduzioniSpagnolo[getIndiceComando(merce)]) + " *";
          Serial.println("Aggiunta traduzione spagnola per: " + merce);
        }
        if (lang == "/de") {
          merceConLingue += " * " + String(traduzioniTedesco[getIndiceComando(merce)]) + " *";
          Serial.println("Aggiunta traduzione tedesca per: " + merce);
        }
        if (lang == "/fr") {
          merceConLingue += " * " + String(traduzioniFrancese[getIndiceComando(merce)]) + " *";
          Serial.println("Aggiunta traduzione francese per: " + merce);
        }
        if (lang == "/ru") {
          merceConLingue += " * " + String(traduzioniRusso[getIndiceComando(merce)]) + " *";
          Serial.println("Aggiunta traduzione russa per: " + merce);
        }
      }
      return merceConLingue;
    };

    // Applica la funzione a merce1 e merce2
    String merce1ConLingue = aggiungiTraduzioni(merce1);
    String merce2ConLingue = aggiungiTraduzioni(merce2);

    // Sostituisci nella risposta finale
    finalOutput.replace(merce1, merce1ConLingue);
    if (merce2 != "") finalOutput.replace(merce2, merce2ConLingue);

    Serial.println("Traduzioni completate: " + merce1ConLingue + " e " + merce2ConLingue);
  }

  Serial.println("Output finale generato: " + finalOutput);
  return finalOutput;
}

void executeSchedule() {
  Serial.println("\n------- INIZIO EXECUTE SCHEDULE -------");
  Serial.println("Orario corrente: " + String(timeinfo.tm_hour) + ":" + String(timeinfo.tm_min));
  Serial.println("Orario schedulato: " + String(scheduledHour) + ":" + String(scheduledMinute));
  Serial.println("Stato isPrimoSet: " + String(isPrimoSet));
  Serial.println("Stato firstExecution: " + String(firstExecution));

  String lingueTrovate[MAX_LINGUE] = { "" };
  int numeroLingue = 0;

  Serial.println("\nControllo lingue richieste:");

  // Controlla le posizioni 4-7 dell'array scheduledMerci per le lingue
  for (int i = 4; i < 8 && numeroLingue < MAX_LINGUE; i++) {
    if (scheduledMerci[i].length() > 0) {
      for (int j = 0; j < MAX_LINGUE; j++) {
        if (scheduledMerci[i] == LINGUE_SUPPORTATE[j]) {
          lingueTrovate[numeroLingue] = LINGUE_SUPPORTATE[j];
          numeroLingue++;
          Serial.println("Lingua trovata in posizione " + String(i) + ": " + scheduledMerci[i]);
          break;
        }
      }
    }
  }

  Serial.println("Totale lingue trovate: " + String(numeroLingue));

  if (isSingleExecution) {
    Serial.println("Esecuzione comando singolo");
    sendResponseToAll(scheduledCommand);
    isScheduleActive = false;
    return;
  }

  String luogoMaiuscolo = scheduledLuogo;
  luogoMaiuscolo.toUpperCase();
  String output = luogoMaiuscolo + "\n\n";

  if (isPrimoSet) {
    Serial.println("Costruzione primo set (merci 1-2)");
    if (scheduledMerci[0].length() > 0) {
      Serial.println("Elaborazione prima merce con traduzioni");
      String merce1ConTraduzioni = scheduledMerci[0];
      int indice = getIndiceComando(scheduledMerci[0]);
      if (indice != -1) {
        Serial.println("Aggiunta traduzioni per merce1:");
        for (int i = 0; i < numeroLingue; i++) {
          if (lingueTrovate[i] == "es") {
            merce1ConTraduzioni += " * " + String(traduzioniSpagnolo[indice]) + " *";
            Serial.println("- Aggiunta traduzione spagnola");
          }
          if (lingueTrovate[i] == "de") {
            merce1ConTraduzioni += " * " + String(traduzioniTedesco[indice]) + " *";
            Serial.println("- Aggiunta traduzione tedesca");
          }
          if (lingueTrovate[i] == "fr") {
            merce1ConTraduzioni += " * " + String(traduzioniFrancese[indice]) + " *";
            Serial.println("- Aggiunta traduzione francese");
          }
          if (lingueTrovate[i] == "ru") {
            merce1ConTraduzioni += " * " + String(traduzioniRusso[indice]) + " *";
            Serial.println("- Aggiunta traduzione russa");
          }
        }
      }
      output += merce1ConTraduzioni;

      if (scheduledMerci[1].length() > 0) {
        Serial.println("Elaborazione seconda merce con traduzioni");
        String merce2ConTraduzioni = scheduledMerci[1];
        indice = getIndiceComando(scheduledMerci[1]);
        if (indice != -1) {
          Serial.println("Aggiunta traduzioni per merce2:");
          for (int i = 0; i < numeroLingue; i++) {
            if (lingueTrovate[i] == "es") {
              merce2ConTraduzioni += " * " + String(traduzioniSpagnolo[indice]) + " *";
              Serial.println("- Aggiunta traduzione spagnola");
            }
            if (lingueTrovate[i] == "de") {
              merce2ConTraduzioni += " * " + String(traduzioniTedesco[indice]) + " *";
              Serial.println("- Aggiunta traduzione tedesca");
            }
            if (lingueTrovate[i] == "fr") {
              merce2ConTraduzioni += " * " + String(traduzioniFrancese[indice]) + " *";
              Serial.println("- Aggiunta traduzione francese");
            }
            if (lingueTrovate[i] == "ru") {
              merce2ConTraduzioni += " * " + String(traduzioniRusso[indice]) + " *";
              Serial.println("- Aggiunta traduzione russa");
            }
          }
        }
        output += "\n\n" + merce2ConTraduzioni;
      }
    }
  } else {
    Serial.println("Costruzione secondo set (merci 3-4)");
    if (scheduledMerci[2].length() > 0) {
      Serial.println("Elaborazione terza merce con traduzioni");
      String merce3ConTraduzioni = scheduledMerci[2];
      int indice = getIndiceComando(scheduledMerci[2]);
      if (indice != -1) {
        Serial.println("Aggiunta traduzioni per merce3:");
        for (int i = 0; i < numeroLingue; i++) {
          if (lingueTrovate[i] == "es") {
            merce3ConTraduzioni += " * " + String(traduzioniSpagnolo[indice]) + " *";
            Serial.println("- Aggiunta traduzione spagnola");
          }
          if (lingueTrovate[i] == "de") {
            merce3ConTraduzioni += " * " + String(traduzioniTedesco[indice]) + " *";
            Serial.println("- Aggiunta traduzione tedesca");
          }
          if (lingueTrovate[i] == "fr") {
            merce3ConTraduzioni += " * " + String(traduzioniFrancese[indice]) + " *";
            Serial.println("- Aggiunta traduzione francese");
          }
          if (lingueTrovate[i] == "ru") {
            merce3ConTraduzioni += " * " + String(traduzioniRusso[indice]) + " *";
            Serial.println("- Aggiunta traduzione russa");
          }
        }
      }
      output += merce3ConTraduzioni;

      if (scheduledMerci[3].length() > 0) {
        Serial.println("Elaborazione quarta merce con traduzioni");
        String merce4ConTraduzioni = scheduledMerci[3];
        indice = getIndiceComando(scheduledMerci[3]);
        if (indice != -1) {
          Serial.println("Aggiunta traduzioni per merce4:");
          for (int i = 0; i < numeroLingue; i++) {
            if (lingueTrovate[i] == "es") {
              merce4ConTraduzioni += " * " + String(traduzioniSpagnolo[indice]) + " *";
              Serial.println("- Aggiunta traduzione spagnola");
            }
            if (lingueTrovate[i] == "de") {
              merce4ConTraduzioni += " * " + String(traduzioniTedesco[indice]) + " *";
              Serial.println("- Aggiunta traduzione tedesca");
            }
            if (lingueTrovate[i] == "fr") {
              merce4ConTraduzioni += " * " + String(traduzioniFrancese[indice]) + " *";
              Serial.println("- Aggiunta traduzione francese");
            }
            if (lingueTrovate[i] == "ru") {
              merce4ConTraduzioni += " * " + String(traduzioniRusso[indice]) + " *";
              Serial.println("- Aggiunta traduzione russa");
            }
          }
        }
        output += "\n\n" + merce4ConTraduzioni;
      }
    }
  }

  Serial.println("Invio output finale con traduzioni: \n" + output);
  sendResponseToAll(output);

  if (isPrimoSet) {
    Serial.println("Gestione primo set");
    if (firstExecution) {
      Serial.println("Calcolo prossimo orario (primo intervallo: " + String(scheduleIntervalMinutes) + " minuti)");
      scheduledMinute = (scheduledMinute + scheduleIntervalMinutes) % 60;
      scheduledHour = (scheduledHour + (scheduledMinute + scheduleIntervalMinutes) / 60) % 24;
      firstExecution = false;
    } else {
      Serial.println("Calcolo prossimo orario (intervallo standard: " + String(secondIntervalMinutes) + " minuti)");
      scheduledMinute = (scheduledMinute + secondIntervalMinutes) % 60;
      scheduledHour = (scheduledHour + (scheduledMinute + secondIntervalMinutes) / 60) % 24;
    }
    Serial.println("Nuovo orario: " + String(scheduledHour) + ":" + String(scheduledMinute));
    isPrimoSet = false;
  } else {
    Serial.println("Gestione secondo set");
    if (secondIntervalMinutes > 0) {
      scheduledMinute = (scheduledMinute + secondIntervalMinutes) % 60;
      scheduledHour = (scheduledHour + (scheduledMinute + secondIntervalMinutes) / 60) % 24;
    } else {
      scheduledHour = (scheduledHour + 1) % 24;
      scheduledMinute = originalMinute;
    }
    isPrimoSet = true;
    Serial.println("Prossima esecuzione: " + String(scheduledHour) + ":" + String(scheduledMinute));
  }

  Serial.println("Stato finale:");
  Serial.println("isPrimoSet: " + String(isPrimoSet));
  Serial.println("firstExecution: " + String(firstExecution));
  Serial.println("Ora prossima esecuzione: " + String(scheduledHour) + ":" + String(scheduledMinute));
  Serial.println("------- FINE EXECUTE SCHEDULE -------\n");
}

void processTimeScheduleCommand(const String& text) {
  // Reset delle variabili all'inizio della funzione
  isScheduleActive = false;
  isSingleExecution = false;
  isPrimoSet = true;
  firstExecution = true;
  scheduledHour = -1;
  scheduledMinute = -1;
  originalMinute = 0;
  scheduleIntervalMinutes = 60;  // Default 60 minuti
  secondIntervalMinutes = 60;    // Default 60 minuti
  scheduledCommand = "";
  scheduledLuogo = "";

  // Inizializzazione array completo
  for (int i = 0; i < 20; i++) {  // 4 merci + 4 lingue + 1 luogo + 2 timer
    scheduledMerci[i] = "";
  }

  const int MAX_PARTS = 20;
  String parts[MAX_PARTS];
  int partCount = 0;

  Serial.println("\n------- INIZIO ELABORAZIONE COMANDO SCHEDULE -------");
  Serial.println("Comando schedule ricevuto: " + text);

  // Conteggio spazi nel comando
  int spaceCount = 0;
  for (int i = 0; i < text.length(); i++) {
    if (text.charAt(i) == ' ') spaceCount++;
  }
  Serial.println("Numero spazi nel comando: " + String(spaceCount));

  // Gestione comando singolo
  if (spaceCount == 2) {
    Serial.println("Rilevato comando schedule singolo");
    String parts[3];
    int start = 0;
    int end;
    int partCount = 0;

    Serial.println("Analisi parti comando singolo:");
    while (start < text.length() && partCount < 3) {
      end = text.indexOf(' ', start);
      if (end == -1) end = text.length();
      parts[partCount] = text.substring(start, end);
      Serial.println("Elemento " + String(partCount) + ": " + parts[partCount]);
      partCount++;
      start = end + 1;
    }

    int colonPos = parts[0].indexOf(':');
    Serial.println("Posizione dei due punti nell'orario: " + String(colonPos));

    if (colonPos != -1 && colonPos < parts[0].length() - 1) {
      scheduledHour = parts[0].substring(0, colonPos).toInt();
      scheduledMinute = parts[0].substring(colonPos + 1).toInt();
      String formattedMinute = (scheduledMinute < 10) ? "0" + String(scheduledMinute) : String(scheduledMinute);
      Serial.println("Orario impostato - Ora: " + String(scheduledHour) + ", Minuti: " + formattedMinute);
      Serial.println("Minuto di riferimento salvato: " + formattedMinute);

      if (scheduledHour >= 0 && scheduledHour <= 23 && scheduledMinute >= 0 && scheduledMinute <= 59) {
        Serial.println("Orario valido, ricerca merce e luogo in corso");
        String merce = trovaRisposta(parts[1]);
        String luogo = trovaRisposta(parts[2]);
        Serial.println("Merce identificata: " + merce);
        Serial.println("Luogo identificato: " + luogo);

        if (merce != "Comando non riconosciuto" && luogo != "Comando non riconosciuto") {
          String luogoMaiuscolo = luogo;
          luogoMaiuscolo.toUpperCase();
          scheduledCommand = luogoMaiuscolo + "\n\n" + merce;
          isScheduleActive = true;
          isSingleExecution = true;
          String confirmMsg = "Programmazione confermata per le " + parts[0] + "\n" + luogoMaiuscolo + " con " + parts[1];
          bot.sendMessage(SOURCE_GROUP_ID, confirmMsg, "");
          Serial.println("Schedule singolo configurato con successo per: " + parts[0]);
          return;
        }
      }
    }
  }

  Serial.println("Avvio elaborazione schedule multiplo");
  int start = 0;
  int end;

  Serial.println("Analisi elementi comando multiplo:");
  while (start < text.length() && partCount < MAX_PARTS) {
    end = text.indexOf(' ', start);
    if (end == -1) end = text.length();
    parts[partCount] = text.substring(start, end);
    Serial.println("Elemento " + String(partCount) + ": " + parts[partCount]);
    partCount++;
    start = end + 1;
  }
  Serial.println("Totale elementi trovati: " + String(partCount));

  if (partCount < 6) {
    String errorMsg = "Formato comando non valido. Struttura corretta: HH:MM merce1 merce2 merce3 merce4 luogo [intervallo1]";
    bot.sendMessage(SOURCE_GROUP_ID, errorMsg, "");
    Serial.println("ERRORE: " + errorMsg);
    return;
  }

  int colonPos = parts[0].indexOf(':');
  Serial.println("Verifica formato orario - Posizione due punti: " + String(colonPos));

  if (colonPos == -1 || parts[0].length() != 5) {
    String errorMsg = "Formato orario non corretto. Usa HH:MM (esempio: 14:00)";
    bot.sendMessage(SOURCE_GROUP_ID, errorMsg, "");
    Serial.println("ERRORE: " + errorMsg);
    return;
  }

  scheduledHour = parts[0].substring(0, colonPos).toInt();
  scheduledMinute = parts[0].substring(colonPos + 1).toInt();
  originalMinute = scheduledMinute;
  String formattedMinute = (scheduledMinute < 10) ? "0" + String(scheduledMinute) : String(scheduledMinute);
  Serial.println("Orario elaborato - Ora: " + String(scheduledHour));
  Serial.println("Minuti elaborati: " + formattedMinute);
  Serial.println("Minuto di riferimento salvato: " + formattedMinute);

  if (scheduledHour < 0 || scheduledHour > 23 || scheduledMinute < 0 || scheduledMinute > 59) {
    String errorMsg = "Orario specificato non valido.";
    bot.sendMessage(SOURCE_GROUP_ID, errorMsg, "");
    Serial.println("ERRORE: Orario non compreso nel range valido");
    return;
  }

  Serial.println("Elaborazione luogo e merci:");
  scheduledLuogo = trovaRisposta(parts[5]);
  scheduledMerci[8] = scheduledLuogo;  // Salvataggio luogo in posizione 8
  Serial.println("Luogo identificato: " + scheduledLuogo);

  for (int i = 0; i < 4; i++) {
    scheduledMerci[i] = trovaRisposta(parts[i + 1]);
    Serial.println("Merce " + String(i + 1) + " identificata: " + scheduledMerci[i]);
  }

  // Analisi dei timer personalizzati
  Serial.println("\nConfigurazione timer:");
  if (partCount >= 7) {
    scheduleIntervalMinutes = parts[6].toInt();
    scheduledMerci[9] = parts[6];  // Salvataggio primo timer
    Serial.println("Primo intervallo impostato: " + String(scheduleIntervalMinutes) + " minuti");
    secondIntervalMinutes = 60;
    scheduledMerci[10] = "60";
    Serial.println("Secondo intervallo fisso: 60 minuti");
  } else {
    scheduleIntervalMinutes = 60;
    secondIntervalMinutes = 60;
    scheduledMerci[9] = "60";
    scheduledMerci[10] = "60";
    Serial.println("Intervalli impostati default: 60 minuti");
  }

  // Rilevamento lingue richieste
  Serial.println("\nRicerca lingue disponibili:");
  int linguaIndex = 4;
  for (int i = 7; i < partCount; i++) {
    if (parts[i] == "de" || parts[i] == "fr" || parts[i] == "ru" || parts[i] == "es") {
      scheduledMerci[linguaIndex] = parts[i];
      Serial.println("Lingua rilevata: '" + parts[i] + "' - Salvata in posizione " + String(linguaIndex));
      linguaIndex++;
      if (linguaIndex > 7) {
        Serial.println("Raggiunto limite massimo di 4 lingue");
        break;
      }
    }
  }

  Serial.println("\nRiepilogo lingue configurate:");
  for (int i = 4; i < 8; i++) {
    if (scheduledMerci[i] != "") {
      Serial.println("Posizione " + String(i) + ": " + scheduledMerci[i]);
    }
  }

  isScheduleActive = true;
  isSingleExecution = false;
  Serial.println("Schedule attivato - Modalità: multipla");

  String confirmationMessage = "Schedule configurato per le " + parts[0] + "\n";
  confirmationMessage += "Prima esecuzione: " + parts[0] + "\n";
  confirmationMessage += "Secondo invio dopo: " + String(scheduleIntervalMinutes) + " minuti\n";
  confirmationMessage += "Poi ogni ora\n";

  confirmationMessage += "\nSequenza programmata:\n";
  confirmationMessage += "1. " + scheduledLuogo + "\n";
  for (int i = 0; i < 4; i++) {
    if (scheduledMerci[i] != "") {
      confirmationMessage += String(i + 2) + ". " + scheduledMerci[i] + "\n";
    }
  }

  confirmationMessage += "\nLingue attivate: ";
  for (int i = 4; i < 8; i++) {
    if (scheduledMerci[i] != "") {
      confirmationMessage += scheduledMerci[i] + " ";
    }
  }

  bot.sendMessage(SOURCE_GROUP_ID, confirmationMessage, "");
  Serial.println("Schedule multiplo configurato correttamente");
  Serial.println("Messaggio di conferma inviato:");
  Serial.println(confirmationMessage);
  Serial.println("------- FINE ELABORAZIONE COMANDO SCHEDULE -------\n");
}