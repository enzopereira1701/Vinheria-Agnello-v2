# Vinheria-Agnello-v2
# 🌡️ Data Logger Ambiental — Arduino

Monitor e registrador de temperatura, umidade e luminosidade com display LCD, RTC e armazenamento em EEPROM.

---

## 📋 Descrição do Projeto

Este projeto é um **data logger ambiental** desenvolvido com Arduino, capaz de monitorar em tempo real três variáveis do ambiente:

- 🌡️ **Temperatura** — sensor DHT22
- 💧 **Umidade relativa do ar** — sensor DHT22
- ☀️ **Luminosidade** — sensor LDR

Os dados são exibidos em um display LCD 16x2 com I2C, armazenados na EEPROM do Arduino com timestamp via módulo RTC DS1307, e o sistema emite alertas visuais (LEDs) e sonoros (buzzer) quando os valores saem da faixa ideal.

---

## 🔧 Componentes Utilizados

| Componente | Descrição |
|---|---|
| Arduino Uno/Nano | Microcontrolador principal |
| DHT22 | Sensor de temperatura e umidade |
| LDR | Sensor de luminosidade |
| LCD 16x2 I2C (0x27) | Display de exibição |
| RTC DS1307 | Relógio de tempo real |
| LED Verde | Ambiente dentro do ideal |
| LED Amarelo | Alerta — nível intermediário |
| LED Vermelho | Problema — nível crítico |
| Buzzer | Alerta sonoro |
| Resistores | Conforme esquema do circuito |

---

## 📐 Esquema do Circuito

Consulte a imagem `circuito.png` na pasta `/circuito` deste repositório.

**Pinagem principal:**

| Pino Arduino | Componente |
|---|---|
| A0 | Teclado analógico (keypad) |
| A1 | LDR |
| D2 | DHT22 (data) |
| D10 | Buzzer |
| D11 | LED Vermelho |
| D12 | LED Amarelo |
| D13 | LED Verde |
| SDA/SCL | LCD I2C + RTC DS1307 |

---

## 💾 Como os Dados São Armazenados

O sistema utiliza a **EEPROM interna do Arduino** (1 KB) para persistir configurações e logs.

**Mapa de memória:**

| Endereço | Conteúdo |
|---|---|
| 0 | Idioma selecionado |
| 1 | UTC offset (fuso horário) |
| 2 | Índice da região |
| 10 em diante | Registros de log |

**Cada registro de log ocupa 9 bytes:**

| Bytes | Tipo | Dado |
|---|---|---|
| 0–3 | `uint32_t` | Timestamp Unix (data/hora) |
| 4–5 | `int16_t` | Temperatura × 100 |
| 6–7 | `int16_t` | Umidade × 100 |
| 8 | `uint8_t` | Luminosidade (%) |

- Capacidade: **até 109 registros**
- Frequência: **1 gravação por minuto**
- Os dados gravados são a **média de 10 leituras** (amostras coletadas ao longo de 10 segundos), evitando ruídos e leituras instáveis
- Ao atingir o limite, o buffer circular sobrescreve os registros mais antigos

---

## 🚦 Faixas Ideais e Alertas

| Sensor | Faixa Ideal | Alerta | Problema |
|---|---|---|---|
| Temperatura | 10 °C – 16 °C | — | Fora da faixa → LED vermelho + buzzer |
| Umidade | 60% – 80% | — | Fora da faixa → LED vermelho + buzzer |
| Luminosidade | 0% – 21% | 22%–41% → LED amarelo | > 41% → LED vermelho + buzzer intenso |

**Níveis de buzzer:**
- ⚠️ Alerta: beep a cada **3 segundos**
- 🚨 Crítico: beep a cada **1,5 segundos**

---

## 🖥️ Manual de Operação

### Primeiro uso
Ao ligar pela primeira vez (ou após reset), o sistema exibe:
1. Tela de logo animada
2. Seleção de **idioma** (Português / English / Español)
3. Seleção de **fuso horário / região**
4. Menu principal

### Navegação pelo teclado

| Botão | Função |
|---|---|
| ▲ UP - Botão azul | Subir no menu |
| ▼ DOWN - Botão Azul | Descer no menu |
| OK | Confirmar seleção - Botão Verde |
| BACK - Botão Vermehlo| Voltar / cancelar |
| RESET - Botão Preto (segurar por 1,5s) | Acionar reset do sistema |

### Menu Principal

```
=== MENU ===
> Monitorar       ← inicia monitoramento em tempo real
  Ver Log         ← navega pelos registros salvos
  Limpar Log      ← apaga todos os registros
  Calibrar LDR    ← calibra o sensor de luz
  Idioma          ← altera o idioma
  Regiao/Fuso     ← altera o fuso horário
```

### Monitoramento

- Escolha entre **Luminosidade**, **Temperatura** ou **Umidade**
- O display exibe o valor atual e o nível do ambiente (ex: "Amb. Ideal", "Amb. Seco")
- LEDs e buzzer indicam o status automaticamente
- Pressione **BACK** para retornar ao menu

### Ver Log

- Navegue com **▲ / ▼** entre os registros
- Pressione **OK** para alternar entre a página com temperatura/umidade e a página com luminosidade
- Pressione **BACK** para sair

### Calibrar LDR

O processo leva 10 segundos. O sistema mede os valores mínimo e máximo do sensor no ambiente atual, melhorando a precisão da leitura de luminosidade.

### Reset do Sistema

Mantenha o botão **RESET** pressionado por **1,5 segundos**. Confirme com **OK**. Isso apaga as configurações salvas (idioma e fuso horário) e reinicia o fluxo inicial. **Os logs de dados não são apagados pelo reset** — use "Limpar Log" no menu para isso.

---

## 🔗 Links

- **Simulação no Wokwi:** [https://wokwi.com](https://wokwi.com)
- **Vídeo (Manual de como usar):** 

---

## 🛠️ Bibliotecas Necessárias

Instale pela Arduino IDE (Sketch → Incluir Biblioteca → Gerenciar Bibliotecas):

- `LiquidCrystal_I2C`
- `DHT sensor library` (Adafruit)
- `RTClib` (Adafruit)

---

## ⚙️ Configuração Inicial do RTC

Na primeira vez que usar o módulo RTC, descomente a linha abaixo no `setup()`, faça o upload, depois comente novamente e faça um segundo upload:

```cpp
RTC.adjust(DateTime(F(__DATE__), F(__TIME__)));
```

---

## Autores
- Enzo Borgo (RM572529),
- João Araújo (RM571420) 
- Yannick Parreira (RM 572443)