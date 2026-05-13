#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <EEPROM.h>
#include <DHT.h>
#include <RTClib.h>

LiquidCrystal_I2C lcd(0x27, 16, 2);
RTC_DS1307 RTC;

// =======================
// PINOS
// =======================
#define KEYPAD  A0
#define LDR     A1
#define DHTPIN  2
#define DHTTYPE DHT22

DHT dht(DHTPIN, DHTTYPE);

const int greenPin  = 13;
const int yellowPin = 12;
const int redPin    = 11;
const int buzzerPin = 10;

// =======================
// EEPROM
// 0 = idioma       (0-2, 255 = nao salvo)
// 1 = UTC offset+12 (0-26, 255 = nao salvo)
// 2 = regiaoIdx    (0-19, 255 = nao salvo)
// 10+ = logs       (9 bytes cada)
// =======================
const int EEPROM_IDIOMA    = 0;
const int EEPROM_UTC       = 1;
const int EEPROM_REGIAO    = 2;
const int EEPROM_LOG_START = 10;
const int RECORD_SIZE      = 9;
const int MAX_RECORDS      = 109;

const uint32_t TS_MINIMO = 788918400UL;

int      currentLogAddress = EEPROM_LOG_START;
uint32_t lastLoggedTime    = 0;

// =======================
// VARIAVEIS GLOBAIS
// =======================
int8_t  idioma    = -1;
int8_t  utcOffset = -3;
uint8_t regiaoIdx = 255;

uint8_t opcaoIndex    = 0;
bool    animacaoFeita = false;

bool          buzzerLigado  = false;
unsigned long tempoBuzzer   = 0;
bool          buzzerJaTocou = false;

unsigned long tempoReset     = 0;
bool          contandoReset  = false;
bool          resetDisparado = false;

int  ldrMin   = 1023;
int  ldrMax   = 0;
bool calibrado = false;

const uint8_t NUM_AMOSTRAS = 10;
float         amostrasTemp[NUM_AMOSTRAS];
float         amostrasUmid[NUM_AMOSTRAS];
uint8_t       amostrasLux[NUM_AMOSTRAS];
uint8_t       indicAmostra      = 0;
uint8_t       amostrasColetadas = 0;
unsigned long ultimaAmostra = 0;
float         mediaTemp  = 0, mediaUmid = 0;
uint8_t       mediaLux   = 0;
bool          mediaPronta = false;

// =======================
// TRIGGERS
// =======================
const float   TRIGGER_T_MIN   = 10.0;
const float   TRIGGER_T_MAX   = 16.0;
const float   TRIGGER_U_MIN   = 60.0;
const float   TRIGGER_U_MAX   = 80.0;
const uint8_t TRIGGER_LUX_MAX = 21;

// =======================
// REGIOES (PROGMEM)
// =======================
const char rn00[] PROGMEM = "Acre";
const char rn01[] PROGMEM = "Amazonas";
const char rn02[] PROGMEM = "Brasilia/SP";
const char rn03[] PROGMEM = "Noronha";
const char rn04[] PROGMEM = "Cabo Verde";
const char rn05[] PROGMEM = "Lisboa/UTC";
const char rn06[] PROGMEM = "Paris/Roma";
const char rn07[] PROGMEM = "Moscou";
const char rn08[] PROGMEM = "Dubai";
const char rn09[] PROGMEM = "Bangkok";
const char rn10[] PROGMEM = "Toquio";
const char rn11[] PROGMEM = "Sydney";
const char rn12[] PROGMEM = "NYC/Miami";
const char rn13[] PROGMEM = "Chicago";
const char rn14[] PROGMEM = "Denver";
const char rn15[] PROGMEM = "Los Angeles";
const char rn16[] PROGMEM = "Bue. Aires";
const char rn17[] PROGMEM = "Santiago";
const char rn18[] PROGMEM = "Bogota/Lima";
const char rn19[] PROGMEM = "Mexico City";

struct Regiao { const char* nome; int8_t offset; };

const Regiao regioes[] PROGMEM = {
  {rn00,-5},{rn01,-4},{rn02,-3},{rn03,-2},
  {rn04,-1},{rn05, 0},{rn06, 1},{rn07, 3},
  {rn08, 4},{rn09, 7},{rn10, 9},{rn11,10},
  {rn12,-5},{rn13,-6},{rn14,-7},{rn15,-8},
  {rn16,-3},{rn17,-3},{rn18,-5},{rn19,-6},
};
const uint8_t NUM_REGIOES = sizeof(regioes) / sizeof(Regiao);

char regiaoNomeBuf[13];

void getRegiaoNome(uint8_t idx) {
  const char* ptr = (const char*)pgm_read_ptr(&regioes[idx].nome);
  strncpy_P(regiaoNomeBuf, ptr, 12);
  regiaoNomeBuf[12] = '\0';
}

int8_t getRegiaoOffset(uint8_t idx) {
  return (int8_t)pgm_read_byte(&regioes[idx].offset);
}

// =======================
// ICONES
// =======================
byte sol0[8]  = {B00000,B10001,B01110,B11111,B01110,B10001,B00000,B00000};
byte sol1[8]  = {B10001,B01010,B00100,B11111,B00100,B01010,B10001,B00000};
byte term0[8] = {B00100,B01010,B01010,B01010,B01110,B01110,B00100,B00000};
byte term1[8] = {B00100,B01110,B01110,B01110,B01110,B11111,B01110,B00000};
byte gota0[8] = {B00100,B00100,B01010,B10001,B10001,B10001,B01110,B00000};
byte gota1[8] = {B00100,B01110,B01010,B10001,B10001,B10001,B01110,B00000};
byte roda0[8] = {B00100,B00100,B10101,B01110,B10101,B00100,B00100,B00000};
byte roda1[8] = {B10101,B01110,B00100,B10101,B00100,B01110,B10101,B00000};

// =======================
// PROTOTIPOS
// =======================
void pararBuzzer();
void menuPrincipal();
void telaMonitorando();
void fluxoInicialCompleto();
void executarReset();

// =======================
// HELPERS DE IDIOMA
// =======================
bool usarFahrenheit() { return (idioma == 1); }

float tempParaExibir(float celsius) {
  return usarFahrenheit() ? celsius * 9.0f / 5.0f + 32.0f : celsius;
}

const __FlashStringHelper* txt(
    const __FlashStringHelper* pt,
    const __FlashStringHelper* en,
    const __FlashStringHelper* es)
{
  if (idioma == 1) return en;
  if (idioma == 2) return es;
  return pt;
}

const __FlashStringHelper* txtOkSim()    { return txt(F("OK=Sim BACK=Nao"), F("OK=Yes BACK=No"),  F("OK=Si  BACK=No")); }
const __FlashStringHelper* txtSalvo()    { return txt(F("Salvo!"),           F("Saved!"),           F("Guardado!")); }
const __FlashStringHelper* txtPronto()   { return txt(F("Pronto!"),          F("Done!"),            F("Listo!")); }
const __FlashStringHelper* txtBemVindo() { return txt(F("Bem-vindo!"),       F("Welcome!"),         F("Bienvenido!")); }
const __FlashStringHelper* txtEscolher() { return txt(F("Escolha:"),         F("Select:"),          F("Elegir:")); }
const __FlashStringHelper* txtAnimacao() { return txt(F("O que monitorar?"), F("What to monitor?"), F("Que monitorear?")); }
const __FlashStringHelper* txtCancelado(){ return txt(F("Cancelado"),        F("Cancelled"),        F("Cancelado")); }

// =======================
// BOTOES
// =======================
char lerBotao() {
  int v = analogRead(KEYPAD);
  if (v < 50)              return 'O';
  if (v >= 100 && v < 200) return 'U';
  if (v >= 250 && v < 380) return 'D';
  if (v >= 430 && v < 560) return 'B';
  if (v >= 680 && v < 780) return 'R';
  return 'N';
}

void aguardarSoltar() {
  while (true) {
    char b = lerBotao();
    if (b == 'N' || b == 'R') break;
    delay(20);
  }
  delay(30);
}

char esperarBotao(unsigned long timeout = 0) {
  unsigned long inicio = millis();
  while (true) {
    char b = lerBotao();
    if (b != 'N' && b != 'R') {
      aguardarSoltar();
      return b;
    }
    if (timeout > 0 && millis() - inicio >= timeout) return 'N';
    delay(30);
  }
}

// =======================
// BUZZER
// =======================
void pararBuzzer() {
  digitalWrite(buzzerPin, HIGH);
  buzzerLigado  = false;
  buzzerJaTocou = false;
  tempoBuzzer   = 0;
}

void beepCurto() {
  digitalWrite(buzzerPin, LOW);
  delay(80);
  digitalWrite(buzzerPin, HIGH);
}

void beepLongo() {
  digitalWrite(buzzerPin, LOW);
  delay(350);
  digitalWrite(buzzerPin, HIGH);
}

// =======================
// RESET
// =======================
void verificarReset() {
  bool pressionado = (lerBotao() == 'R');

  if (pressionado) {
    if (!contandoReset) {
      contandoReset  = true;
      resetDisparado = false;
      tempoReset     = millis();
    }
    if (!resetDisparado && millis() - tempoReset >= 1500) {
      resetDisparado = true;
      executarReset();
    }
  } else {
    contandoReset  = false;
    resetDisparado = false;
  }
}

void executarReset() {
  pararBuzzer();

  digitalWrite(greenPin,  LOW);
  digitalWrite(yellowPin, LOW);
  digitalWrite(redPin,    HIGH);
  beepCurto();

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(txt(F("Confirmar reset?"), F("Confirm reset?"), F("Confirmar reset?")));
  lcd.setCursor(0, 1);
  lcd.print(txtOkSim());

  while (lerBotao() != 'N') delay(20);
  delay(200);

  char resp = esperarBotao(0);

  if (resp != 'O') {
    digitalWrite(redPin, LOW);
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(txtCancelado());
    delay(1000);
    lcd.clear();
    contandoReset  = false;
    resetDisparado = false;
    return;
  }

  beepLongo();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(txt(F("Resetando..."), F("Resetting..."), F("Restableciendo...")));
  delay(1500);

  digitalWrite(redPin, LOW);

  EEPROM.write(EEPROM_IDIOMA, 255);
  EEPROM.write(EEPROM_UTC,    255);
  EEPROM.write(EEPROM_REGIAO, 255);
  idioma    = -1;
  utcOffset = -3;
  regiaoIdx = 255;

  contandoReset  = false;
  resetDisparado = false;

  fluxoInicialCompleto();
}

// =======================
// RTC
// =======================
DateTime horaAtual() {
  DateTime now = RTC.now();
  return DateTime((uint32_t)(now.unixtime() + (long)utcOffset * 3600L));
}

// =======================
// EEPROM — LOG
// =======================
void salvarLog(float temp, float umid, uint8_t lux) {
  uint32_t ts      = horaAtual().unixtime();
  int16_t  tempInt = (int16_t)(temp * 100);
  int16_t  umidInt = (int16_t)(umid * 100);

  EEPROM.put(currentLogAddress,     ts);
  EEPROM.put(currentLogAddress + 4, tempInt);
  EEPROM.put(currentLogAddress + 6, umidInt);
  EEPROM.put(currentLogAddress + 8, lux);

  currentLogAddress += RECORD_SIZE;
  if (currentLogAddress >= EEPROM_LOG_START + MAX_RECORDS * RECORD_SIZE)
    currentLogAddress = EEPROM_LOG_START;
}

int contarRegistros() {
  int count = 0;
  for (int addr = EEPROM_LOG_START;
       addr < EEPROM_LOG_START + MAX_RECORDS * RECORD_SIZE;
       addr += RECORD_SIZE)
  {
    uint32_t ts;
    EEPROM.get(addr, ts);
    if (ts != 0xFFFFFFFF && ts != 0 && ts >= TS_MINIMO) count++;
  }
  return count;
}

void lerRegistroN(int n, uint32_t &ts, int16_t &ti, int16_t &ui, uint8_t &lx) {
  int found = 0;
  for (int addr = EEPROM_LOG_START;
       addr < EEPROM_LOG_START + MAX_RECORDS * RECORD_SIZE;
       addr += RECORD_SIZE)
  {
    uint32_t t;
    EEPROM.get(addr, t);
    if (t == 0xFFFFFFFF || t == 0 || t < TS_MINIMO) continue;
    if (found == n) {
      int16_t a, b;
      EEPROM.get(addr,     t);
      EEPROM.get(addr + 4, a);
      EEPROM.get(addr + 6, b);
      uint8_t c = EEPROM.read(addr + 8);  // 1 byte exato, evita desalinhamento
      ts = t; ti = a; ui = b; lx = c;
      return;
    }
    found++;
  }
}

void exibirLog() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(txt(F("Lendo logs..."), F("Reading logs..."), F("Leyendo logs...")));
  delay(800);

  int total = contarRegistros();

  if (total == 0) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(txt(F("Sem registros"), F("No records"), F("Sin registros")));
    delay(2000);
    lcd.clear();
    return;
  }

  int     reg    = 0;
  uint8_t pag    = 0;
  bool    redraw = true;
  char    linha[17];

  while (true) {
    if (redraw) {
      uint32_t ts; int16_t ti, ui; uint8_t lx;
      lerRegistroN(reg, ts, ti, ui, lx);

      DateTime dt(ts);
      float tempExib = tempParaExibir(ti / 100.0f);
      float umid     = ui / 100.0f;
      char  unid     = usarFahrenheit() ? 'F' : 'C';

      lcd.clear();

      snprintf(linha, sizeof(linha), "%02d/%02d %02d:%02d %2d/%d",
               dt.day(), dt.month(), dt.hour(), dt.minute(),
               reg + 1, total);
      lcd.setCursor(0, 0);
      lcd.print(linha);

      lcd.setCursor(0, 1);
      if (pag == 0) {
        char ft[7], fu[5];
        dtostrf(tempExib, 4, 1, ft);
        dtostrf(umid,     4, 0, fu);  // largura 4 para caber "100"
        snprintf(linha, sizeof(linha), "T:%s%c U:%s%%", ft, unid, fu);
      } else {
        const char* lbl = (idioma == 1) ? "Light" : "Luz";
        snprintf(linha, sizeof(linha), "%s: %3d%%  [OK<]", lbl, (int)lx);
      }
      lcd.print(linha);
      redraw = false;
    }

    char b = esperarBotao(0);
    if (b == 'B') break;
    if (b == 'U') { if (reg > 0)         { reg--; pag = 0; redraw = true; } }
    if (b == 'D') { if (reg < total - 1) { reg++; pag = 0; redraw = true; } }
    if (b == 'O') { pag = (pag == 0) ? 1 : 0; redraw = true; }
  }

  lcd.clear();
}

void limparLog() {
  for (int addr = EEPROM_LOG_START;
       addr < EEPROM_LOG_START + MAX_RECORDS * RECORD_SIZE; addr++) {
    EEPROM.write(addr, 0xFF);
  }
  currentLogAddress = EEPROM_LOG_START;
  lastLoggedTime    = 0;
}

void validarOuLimparLog() {
  for (int addr = EEPROM_LOG_START;
       addr < EEPROM_LOG_START + MAX_RECORDS * RECORD_SIZE;
       addr += RECORD_SIZE)
  {
    uint32_t ts;
    EEPROM.get(addr, ts);
    if (ts != 0xFFFFFFFF && ts != 0 && ts < TS_MINIMO) {
      limparLog();
      return;
    }
  }
}

void recuperarLogAddress() {
  for (int addr = EEPROM_LOG_START;
       addr < EEPROM_LOG_START + MAX_RECORDS * RECORD_SIZE;
       addr += RECORD_SIZE)
  {
    uint32_t ts;
    EEPROM.get(addr, ts);
    if (ts == 0xFFFFFFFF) {
      currentLogAddress = addr;
      return;
    }
  }
  currentLogAddress = EEPROM_LOG_START;
}

// =======================
// LDR
// =======================
void calibrarLDR() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(txt(F("Calibrando LDR"), F("Calibrating LDR"), F("Calibrando LDR")));
  lcd.setCursor(0, 1);
  lcd.print(F("[          ]"));

  ldrMin = 1023; ldrMax = 0;
  for (uint8_t i = 0; i < 10; i++) {
    int v = analogRead(LDR);
    if (v < ldrMin) ldrMin = v;
    if (v > ldrMax) ldrMax = v;
    lcd.setCursor(1, 1);
    for (uint8_t b = 0; b < 10; b++) lcd.print(b <= i ? '#' : '.');
    delay(1000);
  }
  if (ldrMax == ldrMin) ldrMax = ldrMin + 1;
  calibrado = true;
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(txtPronto());
  delay(1000);
  lcd.clear();
}

uint8_t lerLux() {
  int raw = analogRead(LDR);
  if (!calibrado) return (uint8_t)constrain(map(raw, 1023, 0, 0, 100), 0, 100);
  return (uint8_t)constrain(map(raw, ldrMax, ldrMin, 0, 100), 0, 100);
}

// =======================
// MEDIAS 10s
// =======================
void atualizarMedias(float t, float u, uint8_t lux) {
  if (millis() - ultimaAmostra < 1000) return;
  ultimaAmostra = millis();
  amostrasTemp[indicAmostra] = t;
  amostrasUmid[indicAmostra] = u;
  amostrasLux[indicAmostra]  = lux;
  indicAmostra = (indicAmostra + 1) % NUM_AMOSTRAS;
  float st = 0, su = 0; uint16_t sl = 0;
  for (uint8_t i = 0; i < NUM_AMOSTRAS; i++) {
    st += amostrasTemp[i]; su += amostrasUmid[i]; sl += amostrasLux[i];
  }
  mediaTemp = st / NUM_AMOSTRAS;
  mediaUmid = su / NUM_AMOSTRAS;
  mediaLux  = (uint8_t)(sl / NUM_AMOSTRAS);
  // Só marca pronta após o buffer estar completamente preenchido,
  // evitando médias distorcidas pelos zeros iniciais.
  if (amostrasColetadas < NUM_AMOSTRAS) amostrasColetadas++;
  mediaPronta = (amostrasColetadas >= NUM_AMOSTRAS);
}

// =======================
// TEXTOS DE OPCOES E NIVEIS
// =======================
void printOpcao(uint8_t i) {
  if (i == 0) lcd.print(txt(F("Luminosidade"), F("Brightness"),  F("Luminosidad")));
  if (i == 1) lcd.print(txt(F("Temperatura"),  F("Temperature"), F("Temperatura")));
  if (i == 2) lcd.print(txt(F("Umidade"),      F("Humidity"),    F("Humedad")));
}

void printNivelLuz(uint8_t pct) {
  if      (pct <= 21) lcd.print(txt(F("Normal       "), F("Normal       "), F("Normal       ")));
  else if (pct <= 41) lcd.print(txt(F("Alerta       "), F("Warning      "), F("Alerta       ")));
  else                lcd.print(txt(F("Problema     "), F("Problem      "), F("Problema     ")));
}

void printNivelTemp(float tempC) {
  if      (tempC < TRIGGER_T_MIN)  lcd.print(txt(F("Amb. Frio    "), F("Cold env.    "), F("Amb. Frio    ")));
  else if (tempC <= TRIGGER_T_MAX) lcd.print(txt(F("Amb. Ideal   "), F("Ideal env.   "), F("Amb. Ideal   ")));
  else                             lcd.print(txt(F("Amb. Quente  "), F("Hot env.     "), F("Amb. Caliente")));
}

void printNivelUmid(float h) {
  if      (h < TRIGGER_U_MIN)  lcd.print(txt(F("Amb. Seco    "), F("Dry env.     "), F("Amb. Seco    ")));
  else if (h <= TRIGGER_U_MAX) lcd.print(txt(F("Amb. Ideal   "), F("Ideal env.   "), F("Amb. Ideal   ")));
  else                         lcd.print(txt(F("Amb. Umido   "), F("Humid env.   "), F("Amb. Humedo  ")));
}

// =======================
// LEDS — por sensor monitorado
// Luminosidade: problema=vermelho, alerta=amarelo, normal=verde
// Temperatura:  fora do ideal=vermelho, ideal=verde
// Umidade:      fora do ideal=vermelho, ideal=verde
// =======================
void atualizarLEDs(uint8_t lux, float tempC, float umid) {
  bool vermelho = false;
  bool amarelo  = false;

  if (opcaoIndex == 0) {
    if      (lux > 41)              vermelho = true;
    else if (lux > TRIGGER_LUX_MAX) amarelo  = true;
  } else if (opcaoIndex == 1) {
    if (tempC > TRIGGER_T_MAX || tempC < TRIGGER_T_MIN) vermelho = true;
  } else {
    if (umid > TRIGGER_U_MAX || umid < TRIGGER_U_MIN) vermelho = true;
  }

  bool verde = !vermelho && !amarelo;
  digitalWrite(greenPin,  verde    ? HIGH : LOW);
  digitalWrite(yellowPin, amarelo  ? HIGH : LOW);
  digitalWrite(redPin,    vermelho ? HIGH : LOW);
}

// =======================
// BUZZER DE MONITORAMENTO
// Alerta  (nivel 1): beep a cada 3s
// Critico (nivel 2): beep a cada 1,5s
// =======================
void gerenciarBuzzer(uint8_t nivel) {
  if (nivel == 0) { pararBuzzer(); return; }

  unsigned long intervalo = (nivel == 2) ? 1500UL : 3000UL;

  if (!buzzerLigado && !buzzerJaTocou) {
    digitalWrite(buzzerPin, LOW);
    buzzerLigado = true; tempoBuzzer = millis();
  }
  if (buzzerLigado && millis() - tempoBuzzer >= 150) {
    digitalWrite(buzzerPin, HIGH);
    buzzerLigado = false; buzzerJaTocou = true; tempoBuzzer = millis();
  }
  if (!buzzerLigado && buzzerJaTocou && millis() - tempoBuzzer >= intervalo) {
    buzzerJaTocou = false;
  }
}

// =======================
// ICONES
// =======================
void carregarSol()        { lcd.createChar(0, sol0);  lcd.createChar(1, sol1); }
void carregarTermometro() { lcd.createChar(0, term0); lcd.createChar(1, term1); }
void carregarGota()       { lcd.createChar(0, gota0); lcd.createChar(1, gota1); }

void desenharIcone(uint8_t col, uint8_t row) {
  lcd.setCursor(col, row);
  lcd.write(byte((millis() / 600) % 2));
}

// =======================
// LOGO
// =======================
void telaLogo() {
  lcd.createChar(0, roda0);
  lcd.createChar(1, roda1);
  for (uint8_t i = 0; i < 8; i++) {
    lcd.clear();
    lcd.setCursor(2, 0); lcd.print(F("marcha.dev"));
    lcd.setCursor(7, 1); lcd.write(byte(i % 2));
    delay(250);
  }
  lcd.clear();
}

// =======================
// ANIMACAO DIGITANDO
// =======================
void textoAnimado(const __FlashStringHelper* texto) {
  lcd.clear();
  PGM_P p = reinterpret_cast<PGM_P>(texto);
  uint8_t col = 0, row = 0;
  while (true) {
    char c = pgm_read_byte(p++);
    if (!c) break;
    if (col == 16) { col = 0; row = 1; }
    lcd.setCursor(col++, row);
    lcd.print(c);
    delay(45);
  }
  delay(700);
}

// =======================
// SELECIONAR IDIOMA
// =======================
void selecionarIdioma() {
  const __FlashStringHelper* opcoes[3] = {F("Portugues"), F("English"), F("Espanol")};
  uint8_t mi  = (idioma >= 0 && idioma <= 2) ? (uint8_t)idioma : 0;
  uint8_t ult = 255;

  while (true) {
    if (ult != mi) {
      lcd.clear();
      lcd.setCursor(0, 0); lcd.print(F("< Idioma/Lang >"));
      lcd.setCursor(0, 1); lcd.print(F("> ")); lcd.print(opcoes[mi]);
      ult = mi;
    }
    char b = esperarBotao();
    if (b == 'U') { mi = (mi + 2) % 3; }
    if (b == 'D') { mi = (mi + 1) % 3; }
    if (b == 'O') {
      idioma = (int8_t)mi;
      EEPROM.write(EEPROM_IDIOMA, (uint8_t)idioma);
      beepCurto();
      lcd.clear();
      lcd.setCursor(0, 0); lcd.print(txtBemVindo());
      delay(1200);
      lcd.clear();
      return;
    }
  }
}

// =======================
// SELECIONAR REGIAO
// =======================
void selecionarRegiao() {
  uint8_t idx = (regiaoIdx < NUM_REGIOES) ? regiaoIdx : 2;
  uint8_t ult = 255;
  char buf[17];

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(txt(F("< Fuso horario >"), F("< Time Zone >   "), F("< Huso horario >")));
  delay(800);

  while (true) {
    if (ult != idx) {
      getRegiaoNome(idx);
      int8_t off = getRegiaoOffset(idx);
      lcd.setCursor(0, 0);
      snprintf(buf, sizeof(buf), "%-12s      ", regiaoNomeBuf);
      buf[16] = '\0';
      lcd.print(buf);
      lcd.setCursor(0, 1);
      snprintf(buf, sizeof(buf), "UTC%+03d  %2d/%2d   ",
               (int)off, (int)idx + 1, (int)NUM_REGIOES);
      lcd.print(buf);
      ult = idx;
    }
    char b = esperarBotao();
    if (b == 'U') { idx = (idx + NUM_REGIOES - 1) % NUM_REGIOES; }
    if (b == 'D') { idx = (idx + 1) % NUM_REGIOES; }
    if (b == 'O') {
      regiaoIdx = idx;
      utcOffset = getRegiaoOffset(idx);
      EEPROM.write(EEPROM_UTC,    (uint8_t)(utcOffset + 12));
      EEPROM.write(EEPROM_REGIAO, regiaoIdx);
      beepCurto();
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print(txtSalvo());
      delay(1000);
      lcd.clear();
      return;
    }
  }
}

// =======================
// FLUXO INICIAL COMPLETO
// =======================
void fluxoInicialCompleto() {
  telaLogo();
  selecionarIdioma();
  selecionarRegiao();
  menuPrincipal();
}

// =======================
// MONITORAMENTO — NOME DO SENSOR
// =======================
bool mostrarNomeEntrada() {
  lcd.clear();
  lcd.setCursor(0, 0);
  printOpcao(opcaoIndex);
  unsigned long t0 = millis();
  while (millis() - t0 < 1500) {
    char bk = lerBotao();
    if (bk == 'B') { aguardarSoltar(); return false; }
    delay(50);
  }
  lcd.clear();
  return true;
}

// =======================
// MENU MONITORAMENTO
// =======================
void menuMonitoramento() {
  pararBuzzer();
  textoAnimado(txtAnimacao());

  uint8_t ultimoIndex = 255;
  while (true) {
    verificarReset();
    if (ultimoIndex != opcaoIndex) {
      lcd.clear();
      lcd.setCursor(0, 0); lcd.print(txtEscolher());
      lcd.setCursor(0, 1); lcd.print(F("> ")); printOpcao(opcaoIndex);
      ultimoIndex = opcaoIndex;
    }
    char b = esperarBotao(100);
    if (b == 'U') { opcaoIndex = (opcaoIndex + 2) % 3; }
    if (b == 'D') { opcaoIndex = (opcaoIndex + 1) % 3; }
    if (b == 'O') { animacaoFeita = false; telaMonitorando(); ultimoIndex = 255; }
    if (b == 'B') return;
  }
}

// =======================
// MENU PRINCIPAL
// =======================
const uint8_t NUM_ITENS = 6;

void printItemMenu(uint8_t i) {
  switch (i) {
    case 0: lcd.print(txt(F("Monitorar"),    F("Monitor"),    F("Monitorear")));   break;
    case 1: lcd.print(txt(F("Ver Log"),      F("View Log"),   F("Ver Log")));      break;
    case 2: lcd.print(txt(F("Limpar Log"),   F("Clear Log"),  F("Borrar Log")));   break;
    case 3: lcd.print(txt(F("Calibrar LDR"),F("Calib. LDR"), F("Calibrar LDR"))); break;
    case 4: lcd.print(txt(F("Idioma"),       F("Language"),   F("Idioma")));       break;
    case 5: lcd.print(txt(F("Regiao/Fuso"), F("Time Zone"),   F("Huso horario"))); break;
  }
}

void menuPrincipal() {
  uint8_t idx = 0, ultIdx = 255;

  while (true) {
    verificarReset();

    if (ultIdx != idx) {
      lcd.clear();
      lcd.setCursor(0, 0); lcd.print(F("=== MENU ==="));
      lcd.setCursor(0, 1); lcd.print(F("> ")); printItemMenu(idx);
      ultIdx = idx;
    }

    char b = esperarBotao(100);
    if (b == 'U') { idx = (idx + NUM_ITENS - 1) % NUM_ITENS; }
    if (b == 'D') { idx = (idx + 1) % NUM_ITENS; }
    if (b == 'O') {
      switch (idx) {
        case 0: menuMonitoramento(); break;

        case 1: exibirLog(); break;

        case 2:
          lcd.clear();
          lcd.setCursor(0, 0);
          lcd.print(txt(F("Limpar log?"), F("Clear log?"), F("Borrar log?")));
          lcd.setCursor(0, 1);
          lcd.print(txtOkSim());
          {
            char conf = esperarBotao(5000);
            if (conf == 'O') {
              limparLog();
              beepCurto();
              lcd.clear();
              lcd.setCursor(0, 0);
              lcd.print(txtPronto());
              delay(1000);
            }
          }
          lcd.clear();
          break;

        case 3: calibrarLDR(); break;

        case 4: selecionarIdioma(); ultIdx = 255; break;

        case 5: selecionarRegiao(); ultIdx = 255; break;
      }
      ultIdx = 255;
    }
  }
}

// =======================
// TELA MONITORANDO
// LEDs, buzzer e display usam as medias de 10s.
// Nos primeiros 10s (antes de mediaPronta), usa leitura instantanea
// como fallback para nao exibir tela em branco.
// Isso elimina oscilacoes na borda dos limites (ex: 60% alternando
// entre seco/ideal a cada leitura).
// =======================
void telaMonitorando() {
  pararBuzzer();

  // Reinicia buffer de medias para evitar dados de sessao anterior
  for (uint8_t i = 0; i < NUM_AMOSTRAS; i++) {
    amostrasTemp[i] = 0; amostrasUmid[i] = 0; amostrasLux[i] = 0;
  }
  indicAmostra      = 0;
  amostrasColetadas = 0;
  ultimaAmostra     = 0;
  mediaTemp         = 0; mediaUmid = 0; mediaLux = 0;
  mediaPronta       = false;

  if      (opcaoIndex == 0) carregarSol();
  else if (opcaoIndex == 1) carregarTermometro();
  else                      carregarGota();

  if (!animacaoFeita) {
    if (!mostrarNomeEntrada()) return;
    animacaoFeita = true;
  }

  char buf[17];

  while (true) {
    verificarReset();

    if (lerBotao() == 'B') {
      aguardarSoltar();
      animacaoFeita = false; pararBuzzer(); return;
    }

    // Leitura instantanea alimenta o acumulador de medias.
    // DHT22 precisa de >=2s entre leituras; chamar mais rapido retorna NaN.
    static float         tRaw = 0, uRaw = 0;
    static unsigned long ultimaLeituraDHT = 0;
    uint8_t luxRaw = lerLux();

    if (millis() - ultimaLeituraDHT >= 2000) {
      ultimaLeituraDHT = millis();
      float tLido = dht.readTemperature();
      float uLido = dht.readHumidity();
      if (!isnan(tLido)) tRaw = tLido;
      if (!isnan(uLido)) uRaw = uLido;
    }

    atualizarMedias(tRaw, uRaw, luxRaw);

    // Valores usados em tudo: medias se prontas, senao fallback instantaneo
    float   t   = mediaPronta ? mediaTemp : tRaw;
    float   u   = mediaPronta ? mediaUmid : uRaw;
    uint8_t lux = mediaPronta ? mediaLux  : luxRaw;

    atualizarLEDs(lux, t, u);

    // Log a cada minuto com medias
    DateTime dt = horaAtual();
    uint32_t tsAtual = dt.unixtime() / 60;
    if (tsAtual != lastLoggedTime && mediaPronta) {
      lastLoggedTime = tsAtual;
      salvarLog(mediaTemp, mediaUmid, mediaLux);
    }

    float tExib = tempParaExibir(t);
    char  unid  = usarFahrenheit() ? 'F' : 'C';

    // Nivel do buzzer por sensor monitorado
    uint8_t nivelBuzzer = 0;
    if (opcaoIndex == 0) {
      if      (lux > 41)              nivelBuzzer = 2; // problema: 1,5s
      else if (lux > TRIGGER_LUX_MAX) nivelBuzzer = 1; // alerta: 3s
    } else if (opcaoIndex == 1) {
      if (t > TRIGGER_T_MAX || t < TRIGGER_T_MIN) nivelBuzzer = 1; // 3s
    } else {
      if (u > TRIGGER_U_MAX || u < TRIGGER_U_MIN) nivelBuzzer = 1; // 3s
    }
    gerenciarBuzzer(nivelBuzzer);

    if (opcaoIndex == 0) {
      desenharIcone(0, 0);
      lcd.setCursor(2, 0); printNivelLuz(lux);
      lcd.setCursor(0, 1);
      const char* lblLuz = (idioma == 1) ? "Light" : "Luz";
      snprintf(buf, sizeof(buf), "%s: %3d%%        ", lblLuz, (int)lux);
      lcd.print(buf);
    }
    else if (opcaoIndex == 1) {
      desenharIcone(0, 0);
      lcd.setCursor(2, 0); printNivelTemp(t);
      lcd.setCursor(0, 1);
      {
        char fbuf[7];
        dtostrf(tExib, 5, 1, fbuf);
        snprintf(buf, sizeof(buf), "Temp:%s%c     ", fbuf, unid);
      }
      lcd.print(buf);
    }
    else {
      desenharIcone(0, 0);
      lcd.setCursor(2, 0); printNivelUmid(u);
      lcd.setCursor(0, 1);
      {
        char fbuf[7];
        dtostrf(u, 4, 0, fbuf);
        snprintf(buf, sizeof(buf), "Umid:%s%%      ", fbuf);
      }
      lcd.print(buf);
    }

    delay(400);
  }
}

// =======================
// SETUP
// =======================
void setup() {
  pinMode(greenPin,  OUTPUT);
  pinMode(yellowPin, OUTPUT);
  pinMode(redPin,    OUTPUT);
  pinMode(buzzerPin, OUTPUT);
  digitalWrite(buzzerPin, HIGH);

  lcd.init();
  lcd.backlight();
  dht.begin();
  Wire.begin();
  RTC.begin();
  // Acerte o RTC UMA vez, depois comente:
  // RTC.adjust(DateTime(F(__DATE__), F(__TIME__)));

  for (uint8_t i = 0; i < NUM_AMOSTRAS; i++) {
    amostrasTemp[i] = 0; amostrasUmid[i] = 0; amostrasLux[i] = 0;
  }

  uint8_t idiomaRaw = EEPROM.read(EEPROM_IDIOMA);
  uint8_t utcRaw    = EEPROM.read(EEPROM_UTC);
  uint8_t regRaw    = EEPROM.read(EEPROM_REGIAO);

  bool idiomaOk = (idiomaRaw <= 2);
  bool regiaoOk = (regRaw < NUM_REGIOES);

  if (idiomaOk) idioma = (int8_t)idiomaRaw;
  if (regiaoOk) { regiaoIdx = regRaw; utcOffset = getRegiaoOffset(regRaw); }
  else if (utcRaw != 255) utcOffset = (int8_t)((int)utcRaw - 12);

  validarOuLimparLog();
  recuperarLogAddress();

  if (currentLogAddress > EEPROM_LOG_START) {
    int lastAddr = currentLogAddress - RECORD_SIZE;
    uint32_t lastTs;
    EEPROM.get(lastAddr, lastTs);
    if (lastTs != 0xFFFFFFFF && lastTs != 0 && lastTs >= TS_MINIMO) {
      lastLoggedTime = DateTime(lastTs).unixtime() / 60;
    }
  }

  if (!idiomaOk || !regiaoOk) {
    fluxoInicialCompleto();
  } else {
    telaLogo();
    lcd.clear();
    lcd.setCursor(0, 0); lcd.print(txtBemVindo());
    getRegiaoNome(regiaoIdx);
    lcd.setCursor(0, 1); lcd.print(regiaoNomeBuf);
    delay(1500);
    lcd.clear();
    menuPrincipal();
  }
}

// =======================
// LOOP
// =======================
void loop() {}
