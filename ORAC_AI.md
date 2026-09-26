# O.R.A.C. — AI Component

This file contains the **AI component of O.R.A.C. Multitool**, extracted from the working `ORAC.ino` source.

It is provided separately for documentation and reference on GitHub. The main `ORAC.ino` remains the complete working program and is **not modified by this file**.

## AI implementation

The component contains the OpenAI Responses API configuration, AI state, question/answer handling, AI terminal screen, on-screen keyboard, and AI touch handling.

> **Security:** Never publish a real OpenAI API key in a public repository. The placeholder below should be replaced locally when compiling the project.

```cpp
// ============================================================
// O.R.A.C. AI TERMINAL
// Separate implementation file for the O.R.A.C. Multitool sketch.
// Arduino IDE compiles this together with ORAC.ino.
// ============================================================

const char *OPENAI_API_KEY = "YOUR_OPENAI_API_KEY_HERE";
const char *OPENAI_MODEL = "gpt-5.6-luna";
const char *OPENAI_ENDPOINT = "https://api.openai.com/v1/responses";

bool oracAiBusy = false;

String aiQuestion = "WHAT IS A BLACK HOLE?";
String aiAnswer = "PRESS ASK O.R.A.C. TO QUERY THE MACHINE.";


// Compact answer display: five visible lines with simple tap-to-scroll.
#define AI_ANSWER_MAX_LINES 24
#define AI_ANSWER_VISIBLE_LINES 5
String aiAnswerLines[AI_ANSWER_MAX_LINES];
int aiAnswerLineCount = 0;
int aiAnswerScroll = 0;

String aiCleanAnswer(const String &input) {
  String out = input;
  // Keep the small terminal display clean by removing common Markdown marks.
  out.replace("**", "");
  out.replace("__", "");
  out.replace("`", "");
  out.replace("# ", "");
  out.replace("\r", "");
  while (out.indexOf("\n\n") >= 0) out.replace("\n\n", "\n");
  return out;
}

void aiPushAnswerLine(const String &line) {
  if (aiAnswerLineCount < AI_ANSWER_MAX_LINES) {
    aiAnswerLines[aiAnswerLineCount++] = line;
  }
}

void prepareAiAnswerLines() {
  aiAnswerLineCount = 0;
  String text = aiCleanAnswer(aiAnswer);
  tft.setTextFont(1);
  tft.setTextSize(1);

  String line = "";
  String word = "";

  for (int i = 0; i <= text.length(); ++i) {
    char c = (i < (int)text.length()) ? text[i] : ' ';

    if (c == ' ' || c == '\n' || c == '\t') {
      if (word.length()) {
        String candidate = line.length() ? line + " " + word : word;
        if (tft.textWidth(candidate) > 268 && line.length()) {
          aiPushAnswerLine(line);
          line = word;
        } else {
          line = candidate;
        }
        word = "";
      }
      if (c == '\n' && line.length()) {
        aiPushAnswerLine(line);
        line = "";
      }
    } else {
      word += c;
    }
  }

  if (line.length()) aiPushAnswerLine(line);
  if (aiAnswerLineCount == 0) aiPushAnswerLine("NO READABLE ANSWER.");
}

String aiJsonEscape(const String &s) {
  String out;
  out.reserve(s.length() + 16);
  for (size_t i = 0; i < s.length(); ++i) {
    char c = s[i];
    if (c == '\\') out += "\\\\";
    else if (c == '"') out += "\\\"";
    else if (c == '\n') out += "\\n";
    else if (c == '\r') out += "\\r";
    else if (c == '\t') out += "\\t";
    else out += c;
  }
  return out;
}

String aiJsonUnescape(const String &s) {
  String out;
  out.reserve(s.length());
  bool esc = false;
  for (size_t i = 0; i < s.length(); ++i) {
    char c = s[i];
    if (esc) {
      if (c == 'n') out += '\n';
      else if (c == 'r') out += '\r';
      else if (c == 't') out += '\t';
      else out += c;
      esc = false;
    } else if (c == '\\') {
      esc = true;
    } else {
      out += c;
    }
  }
  return out;
}

String aiExtractOutputText(const String &json) {
  // Responses API output contains content objects with type=output_text and text=...
  int typePos = json.indexOf("\"type\":\"output_text\"");
  if (typePos < 0) typePos = json.indexOf("\"type\": \"output_text\"");
  if (typePos < 0) return "";

  int textPos = json.indexOf("\"text\":\"", typePos);
  int prefixLen = 8;
  if (textPos < 0) {
    textPos = json.indexOf("\"text\": \"", typePos);
    prefixLen = 9;
  }
  if (textPos < 0) return "";
  int start = textPos + prefixLen;

  String raw;
  raw.reserve(512);
  bool esc = false;
  for (int i = start; i < (int)json.length(); ++i) {
    char c = json[i];
    if (!esc && c == '"') break;
    raw += c;
    if (esc) esc = false;
    else if (c == '\\') esc = true;
  }
  return aiJsonUnescape(raw);
}


String askOpenAI(const String &question) {
  if (!connectOracWifi()) return "WI-FI NOT READY. O.R.A.C. IS CONNECTING.";

  // ------------------------------------------------------------
  // Diagnostic connection sequence. HTTPClient returns -1 for a
  // connection-level failure, so test DNS/TCP/TLS separately.
  // ------------------------------------------------------------
  oracAiStatus = "DNS / TLS TEST...";
  Serial.println("--- O.R.A.C. OPENAI DIAGNOSTIC ---");
  Serial.print("WiFi SSID: ");
  Serial.println(WiFi.SSID());
  Serial.print("WiFi IP: ");
  Serial.println(WiFi.localIP());
  Serial.print("WiFi RSSI: ");
  Serial.println(WiFi.RSSI());
  Serial.println("Resolving api.openai.com...");

  IPAddress openaiIP;
  if (!WiFi.hostByName("api.openai.com", openaiIP)) {
    Serial.println("DNS FAILED: api.openai.com");
  oracWifiFailSound();
    oracAiStatus = "DNS FAILED";
    return "DNS FAILED: API.OPENAI.COM";
  }
  Serial.print("OpenAI IP: ");
  Serial.println(openaiIP);

  // Release the large menu buffers before TLS. Espressif notes that a stable
  // TLS handshake can require roughly 40-50 KB of temporary free heap.
  releaseMenuGraphicsForAI();
  Serial.print("AI: free heap before TLS = ");
  Serial.println(ESP.getFreeHeap());

  WiFiClientSecure client;
  client.setInsecure(); // Diagnostic only: certificate verification disabled.
  client.setTimeout(15000);

  oracAiStatus = "TLS CONNECT...";
  Serial.println("Opening TLS connection to api.openai.com:443...");
  if (!client.connect("api.openai.com", 443)) {
    Serial.println("TLS/TCP CONNECT FAILED");
    Serial.print("AI: free heap at TLS failure = ");
    Serial.println(ESP.getFreeHeap());
    oracAiStatus = "TLS CONNECT FAILED";
    return "TLS CONNECTION FAILED";
  }
  Serial.println("TLS/TCP CONNECT OK");
  client.stop();

  oracAiStatus = "OPENAI REQUEST...";
  HTTPClient https;
  https.setConnectTimeout(15000);
  https.setTimeout(25000);

  if (!https.begin(client, OPENAI_ENDPOINT)) {
    Serial.println("HTTPS BEGIN FAILED");
    oracAiStatus = "HTTPS BEGIN FAILED";
    return "HTTPS CONNECTION SETUP FAILED";
  }

  https.addHeader("Content-Type", "application/json");
  https.addHeader("Authorization", String("Bearer ") + OPENAI_API_KEY);

  String body = "{\"model\":\"";
  body += OPENAI_MODEL;
  body += "\",\"input\":\"";
  body += aiJsonEscape(question);
  body += "\",\"instructions\":\"You are O.R.A.C., a compact retro computer assistant. Give a direct, useful answer in plain text. Maximum 45 words and 4 short sentences. No Markdown, bullet lists or headings. Keep every reply easy to read on a 320x240 display and suitable for later spoken audio.\",\"max_output_tokens\":100}";

  Serial.print("POST bytes: ");
  Serial.println(body.length());
  int code = https.POST(body);
  String response = https.getString();

  Serial.print("OpenAI HTTP: ");
  Serial.println(code);
  if (response.length()) {
    Serial.print("Response: ");
    Serial.println(response);
  }

  https.end();

  if (code < 200 || code >= 300) {
    if (code == 401) {
      oracAiStatus = "API KEY REJECTED";
      return "API KEY REJECTED";
    }
    if (code == 429) {
      oracAiStatus = "API LIMIT";
      return "API RATE LIMIT / PROJECT LIMIT";
    }
    if (code < 0) {
      oracAiStatus = "HTTP CONNECTION FAILED";
      return "HTTP CONNECTION FAILED";
    }
    oracAiStatus = "OPENAI ERROR";
    return "OPENAI ERROR " + String(code);
  }

  oracAiStatus = "ANSWER RECEIVED";
  String answer = aiExtractOutputText(response);
  if (answer.length() == 0) return "O.R.A.C. RECEIVED NO READABLE ANSWER.";
  return answer;
}

void drawAiWrappedText(const String &text, int x, int y, int maxWidth, int lineHeight) {
  // Retained for compatibility with the rest of the sketch.
  tft.setTextFont(1);
  tft.setTextSize(1);
  String line = "";
  int yy = y;
  for (int i = 0; i < text.length(); i++) {
    char c = text[i];
    if ((c == ' ' || c == '\n') && tft.textWidth(line + c) > maxWidth) {
      tft.setCursor(x, yy); tft.print(line);
      yy += lineHeight; line = "";
    } else if (c == '\n') {
      tft.setCursor(x, yy); tft.print(line);
      yy += lineHeight; line = "";
    } else {
      line += c;
    }
  }
  if (line.length()) { tft.setCursor(x, yy); tft.print(line); }
}

void drawAiAnswerBox() {
  prepareAiAnswerLines();
  int maxScroll = max(0, aiAnswerLineCount - AI_ANSWER_VISIBLE_LINES);
  aiAnswerScroll = constrain(aiAnswerScroll, 0, maxScroll);

  tft.setTextColor(WHITE, BLACK);
  for (int i = 0; i < AI_ANSWER_VISIBLE_LINES; ++i) {
    int idx = aiAnswerScroll + i;
    if (idx >= aiAnswerLineCount) break;
    tft.setCursor(16, 139 + i * 11);
    tft.print(aiAnswerLines[idx]);
  }

  if (maxScroll > 0) {
    tft.setTextColor(GREEN_CLOCK, BLACK);
    tft.setCursor(294, 139); tft.print("^");
    tft.setCursor(294, 181); tft.print("v");
    tft.setTextColor(DARK_GREY, BLACK);
    tft.setCursor(280, 124);
    tft.print(String(aiAnswerScroll + 1) + "/" + String(maxScroll + 1));
  }
}

void aiDrawKey(int x,int y,int w,int h,const char *label,uint16_t c) {
  tft.fillRoundRect(x,y,w,h,3,c);
  tft.drawRoundRect(x,y,w,h,3,LIGHT_GREY);
  tft.setTextFont(1); tft.setTextSize(1);
  tft.setTextColor((c==DARK_GREY)?WHITE:BLACK,c);
  int tw=tft.textWidth(label);
  tft.setCursor(x+(w-tw)/2,y+(h-8)/2);
  tft.print(label);
}

void aiKeyboardAdd(char c) {
  if(aiQuestion.length() < AI_QUESTION_MAX) aiQuestion += c;
}

void drawAiKeyboardScreen() {
  tft.fillScreen(BLACK);
  tft.setTextFont(1); tft.setTextSize(2);
  tft.setTextColor(CYAN_CLOCK,BLACK);
  tft.setCursor(8,3); tft.print("O.R.A.C. INPUT");
  tft.setTextSize(1); tft.setTextColor(PALE_YELLOW,BLACK);
  tft.setCursor(220,8); tft.print(aiKeyboardNumbers ? "123 MODE" : "ABC MODE");

  tft.drawRoundRect(6,29,308,39,4,CYAN_CLOCK);
  tft.setTextColor(WHITE,BLACK);
  String shown=aiQuestion;
  // Show the end of a long question so the cursor is always useful.
  while(tft.textWidth(shown) > 292 && shown.length()>0) shown.remove(0,1);
  tft.setCursor(12,43); tft.print(shown);
  if((millis()/500)%2==0) {
    int cx=12+tft.textWidth(shown);
    if(cx<306) tft.drawFastVLine(cx,41,16,WHITE);
  }

  const char *rowsABC[3]={"QWERTYUIOP","ASDFGHJKL","ZXCVBNM"};
  const char *rows123[3]={"1234567890",".,?!:;+-*/","()'\"_#@$%="};
  const char **rows = aiKeyboardNumbers ? rows123 : rowsABC;
  int rowY[3]={75,109,143};
  for(int r=0;r<3;r++) {
    int n=strlen(rows[r]);
    int w=30;
    int total=n*w;
    int x=(320-total)/2;
    for(int k=0;k<n;k++) {
      char lab[2]={rows[r][k],0};
      aiDrawKey(x+k*w,rowY[r],w-2,30,lab, (r==2)?PALE_YELLOW:CYAN_CLOCK);
    }
  }

  aiDrawKey(5,177,48,28,aiKeyboardNumbers?"ABC":"123",PURPLE_CLOCK);
  aiDrawKey(57,177,126,28,"SPACE",GREEN_CLOCK);
  aiDrawKey(187,177,58,28,"DEL",ORANGE_CLOCK);
  aiDrawKey(249,177,66,28,"CLEAR",RED_CLOCK);
  aiDrawKey(5,209,95,28,"BACK",DARK_GREY);
  aiDrawKey(105,209,105,28,"DONE",PALE_YELLOW);
  aiDrawKey(215,209,100,28,"ASK NOW",GREEN_CLOCK);
}

void handleAiKeyboardTouch(int x,int y) {
  if(y>=29 && y<70) return;

  const char *rowsABC[3]={"QWERTYUIOP","ASDFGHJKL","ZXCVBNM"};
  const char *rows123[3]={"1234567890",".,?!:;+-*/","()'\"_#@$%="};
  const char **rows = aiKeyboardNumbers ? rows123 : rowsABC;
  int rowY[3]={75,109,143};
  for(int r=0;r<3;r++) {
    if(y>=rowY[r] && y<rowY[r]+30) {
      int n=strlen(rows[r]);
      int w=30, start=(320-n*w)/2;
      if(x>=start && x<start+n*w) {
        int k=(x-start)/w;
        if(k>=0 && k<n) { aiKeyboardAdd(rows[r][k]); drawAiKeyboardScreen(); }
      }
      return;
    }
  }

  if(y>=177 && y<205) {
    if(x<53) { aiKeyboardNumbers=!aiKeyboardNumbers; drawAiKeyboardScreen(); return; }
    if(x<185) { aiKeyboardAdd(' '); drawAiKeyboardScreen(); return; }
    if(x<247) { if(aiQuestion.length()) aiQuestion.remove(aiQuestion.length()-1); drawAiKeyboardScreen(); return; }
    aiQuestion=""; drawAiKeyboardScreen(); return;
  }

  if(y>=209) {
    if(x<102) { aiKeyboardActive=false; drawAiScreen(); return; }
    if(x<212) {
      aiKeyboardActive=false; aiAnswerScroll=0; aiAnswer="PRESS ASK O.R.A.C. TO QUERY THE MACHINE."; drawAiScreen(); return;
    }
    aiKeyboardActive=false;
    if(!oracAiBusy) {
      oracAiBusy=true;
      if (oracVolume > 0) oracAiThinking(); aiAnswer="CONTACTING O.R.A.C. CORE..."; aiAnswerScroll=0;
      oracAiStatus="QUERYING OPENAI"; drawAiScreen(); delay(20);
      String result=askOpenAI(aiQuestion);
      oracAiBusy=false; aiAnswer=result;
      if (oracVolume > 0) oracAiDone(); aiAnswerScroll=0; updateAiWifiStatus(); drawAiScreen(); oracBeep(880,45);
    }
  }
}

void drawAiScreen() {
  tft.fillScreen(BLACK);

  tft.setTextFont(1);
  tft.setTextSize(2);
  tft.setTextColor(CYAN_CLOCK, BLACK);
  tft.setCursor(9, 4);
  tft.print("O.R.A.C. AI");
  tft.setTextSize(1);
  tft.setTextColor(PALE_YELLOW, BLACK);
  tft.setCursor(160, 8);
  tft.print("ASK THE MACHINE");
  tft.drawFastHLine(8, 27, 304, DARK_GREY);

  tft.setTextColor(GREEN_CLOCK, BLACK);
  tft.setCursor(12, 35);
  tft.print("WIFI LINK: ");
  tft.setTextColor(oracWifiConnected ? GREEN_CLOCK : ORANGE_CLOCK, BLACK);
  tft.print(oracAiStatus);

  tft.setTextColor(CYAN_CLOCK, BLACK);
  tft.setCursor(12, 53);
  tft.print("YOUR QUESTION");
  tft.drawRoundRect(10, 67, 300, 39, 4, CYAN_CLOCK);
  tft.setTextColor(WHITE, BLACK);
  drawAiWrappedText(aiQuestion, 17, 78, 286, 12);

  tft.setTextColor(PALE_YELLOW, BLACK);
  tft.setCursor(12, 117);
  tft.print("O.R.A.C. SAYS...");
  tft.drawRoundRect(10, 131, 300, 65, 4, DARK_GREY);
  drawAiAnswerBox();

  tft.setTextColor(oracAiBusy ? PALE_YELLOW : ORANGE_CLOCK, BLACK);
  tft.setCursor(12, 196);
  tft.print(oracAiBusy ? "AI TERMINAL: THINKING..." : "AI TERMINAL: TEXT MODE / AUDIO READY LATER");

  lifeDrawButton(10, 207, 88, 27, "INPUT", CYAN_CLOCK);
  lifeDrawButton(104, 207, 104, 27, "ASK O.R.A.C.", GREEN_CLOCK);
  lifeDrawButton(214, 207, 96, 27, "HOME", DARK_GREY);
}

void handleAiTouch(int x, int y) {
  if(aiKeyboardActive) {
    handleAiKeyboardTouch(x,y);
    return;
  }

  // Answer scrolling: upper half moves up, lower half moves down.
  if (y >= 131 && y < 196) {
    prepareAiAnswerLines();
    int maxScroll = max(0, aiAnswerLineCount - AI_ANSWER_VISIBLE_LINES);
    if (maxScroll > 0) {
      if (y < 163) aiAnswerScroll--;
      else aiAnswerScroll++;
      aiAnswerScroll = constrain(aiAnswerScroll, 0, maxScroll);
      drawAiScreen();
    }
    return;
  }

  if (y >= 207) {
    if (x < 100) {
      aiKeyboardActive=true;
      aiKeyboardNumbers=false;
      drawAiKeyboardScreen();
    } else if (x < 212) {
      if (oracAiBusy) return;
      oracAiBusy = true;
      aiAnswer = "CONTACTING O.R.A.C. CORE...";
      oracAiStatus = "QUERYING OPENAI";
      drawAiScreen();
      delay(20);
      String result = askOpenAI(aiQuestion);
      oracAiBusy = false;
      aiAnswer = result;
      aiAnswerScroll = 0;
      updateAiWifiStatus();
      drawAiScreen();
      oracBeep(880, 45);
    } else {
      currentScreen = SCREEN_MENU;
    }
    return;
  }

  if (y >= 67 && y < 106) {
    aiKeyboardActive=true;
    aiKeyboardNumbers=false;
    drawAiKeyboardScreen();
  }
}
```
