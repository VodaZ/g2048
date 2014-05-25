#include <stdint.h>
#include <SeeedTouchScreen.h>
#include <TFTv2.h>
#include <SPI.h>
#include <SD.h>
 
#if defined(__AVR_ATmega1280__) || defined(__AVR_ATmega2560__) // mega
#define YP A2
#define XM A1
#define YM 54
#define XP 57
 
#elif defined(__AVR_ATmega32U4__) //leonardo
#define YP A2
#define XM A1
#define YM 18
#define XP 21
 
#else
#define YP A2
#define XM A1
#define YM 14
#define XP 17
 
#endif
 
//konfigurace
 
TouchScreen ts = TouchScreen(XP, YP, XM, YM);
byte SDpin = 4; 
 
int min_x = 232;
int max_x = 1780;
int min_y = 166;
int max_y = 1826;

Point p; 

//proměnné používané ve hře

byte stat = 0; //0 - úvodní obrazovka, 1 - normální hra, 2 - prohra, 3 - výhra

byte plocha[4][4]; //pole pro uložení hodnot jednotlivých dlaždic plochy

uint16_t barvy[18]; //pole s hodnotami barev

boolean rewrite_all = true; //přepis celé obrazovky
boolean rewrite_game; //přepis herní plochy
boolean moved; //mlelo se s dlaždicemi?
boolean gEnd; //konec hry?
boolean gWin; //výhra?

//pomocné proměnné
byte added = 0; //kolikrát došlo k sečtení dlaždic
byte max_added;
byte len;

File soubor;
 
void setup(){
    //inicializace SD karty a displeje
    Tft.TFTinit();
    SD.begin(SDpin);
    pinMode(53, OUTPUT);
    
    //přiřazení hodnot barvám
    barvy[0] = GRAY1;
    for(int b = 1; b < 18; b++){
        barvy[b] = b * (65535 / 18);
    }
    
    randomSeed(analogRead(0)); //náhodný seed pro generátor
}
 
void loop(){    
    if(rewrite_all){
        Tft.fillRectangle(0,0,239,319, barvy[0]);
    }
    
    //úvodní obrazovka
    if(stat == 0){
        if(rewrite_all){
            Tft.fillRectangle(10,10,220,145, barvy[2]);
            Tft.fillRectangle(10,165,220,145, barvy[2]);      
            Tft.drawString("LOAD", 35, 55, 7, BLACK);
            Tft.drawString("NEW", 55, 210, 7, BLACK);  
            
            rewrite_all = false;
        }
        
        checkP();
        
        if(p.z > 10){
            //tlačítko LOAD
            if(p.y < 160){
                rewrite_all = true;
                rewrite_game = true;
                
                if(!SD.exists("G2048.TXT")){
                    stat = 0; 
                    Tft.fillRectangle(20,95,200,100, barvy[17]);
                    Tft.drawString("ERROR", 28, 123, 6, BLACK); 
                }
                else{
                    stat = 1;
                    soubor = SD.open("G2048.TXT");
                    int i = 0;
                    byte p;
                    
                    for(int i=0; i < 4; i++){ //vyčistíme plochu
                        for(int j = 0; j < 4; j++){
                            plocha[i][j] = 0;
                        }
                    }
                    
                    while(soubor.available()){
                        p = soubor.read();
                        plocha[(i - i%4) /4][i % 4] = p;
                        i++;               
                    }
                    soubor.close();
                }
            }
            
            //tlačítko NEW
            else if(p.y >= 160){
                rewrite_all = true;
                rewrite_game = true;
                moved = false;
                added = 0;
                gEnd = false;
                gWin = false;
                stat = 1;
                
                for(int i=0; i < 4; i++){
                    for(int j = 0; j < 4; j++){
                        plocha[i][j] = 0;
                    }
                }
                
                addTile();
                addTile();
            }
        }
    }
    
    //normální hra
    else if(stat == 1){
        if(rewrite_all){
            rewrite_all = false;
            
            Tft.fillRectangle(10,240,105,70,barvy[10]);
            Tft.fillRectangle(125,240,105,70,barvy[10]);
             
            Tft.drawString("SAVE", 27, 263, 3, BLACK); 
            Tft.drawString("MAIN", 142, 250, 3, BLACK);
            Tft.drawString("MENU", 142, 278, 3, BLACK);
        }
        if(rewrite_game){
            Tft.fillRectangle(10,10,220,220,BLACK);
            
            //zobrazení jedotlivých dlaždic
            for(int x = 0; x < 4; x++){
                for(int y = 0; y < 4; y++){
                    Tft.fillRectangle(15+(x*54),15+(y*54), 49, 49, barvy[plocha[y][x]]);
                    if(plocha[y][x] != 0){
                        if(plocha[y][x] >= 10){
                            len = 0;  
                        }
                        else{
                            len = 10;
                        }  
                        
                        Tft.drawNumber(plocha[y][x], 19+(x*54)+len, 29+(y*54), 3, BLACK);
                    }
                }
            }
            rewrite_game = false;
        } 
        
        checkP();
        
        if(p.z > 10){
            
            //detekce dotyku v oblasti herní  plochy
            if(p.x > 10 && p.x < 220 && p.y > 10 && p.y < 230){
                //směr nahoru
                if(p.x > p.y && p.x < -1*p.y + 240){
                    goUp();
                }
                
                //směr dolů
                else if(p.x < p.y && p.x > -1*p.y + 240){
                    goDown();
                }
                
                //směr doprava
                else if(p.x > p.y && p.x > -1*p.y + 240){
                    goRight();
                }
                
                //směr doleva
                else if(p.x < p.y && p.x < -1*p.y + 240){
                    goLeft();
                }
                
                do{
                    checkP();
                    delay(10);
                }while(p.z > 10);
                
                //kontrola konce hry
                
                gEnd = true; //nastavíme na true a pokud nebude pravda, v následujícím cyklu to změníme
                for(int x = 0; x <= 3; x++){
                    for(int y = 0; y <= 3; y++){
                        if(plocha[y][x] == 0){
                            gEnd = false;
                        }
                        else if(x == 3 && y == 3){
                            //nic   
                            //u dolní pravé dlaždice nic nekontrolujeme  
                        }
                        else if(x == 3){
                            if(plocha[y][3] == plocha[y+1][3] ){
                                gEnd = false;
                            }    
                        }
                        else if(y == 3){
                            if(plocha[3][x] == plocha[3][x+1]){
                                gEnd = false;
                            }
                        }
                        else{
                            //zbylá část pole
                            if(plocha[y][x] == plocha[y][x+1] || plocha[y][x] == plocha[y+1][x]){
                                gEnd = false;
                            }
                        }
                    } 
                }
                
                //kontrola výhry
                gWin = false;
                for(int x = 0; x <= 3; x++){
                    for(int y = 0; y <= 3; y++){
                        if(plocha[y][x] == 17){
                            gWin = true;    
                        }
                    }
                }
                
                //pokud se hnulo s bloky 
                if(moved){
                    moved = false;
                    rewrite_game = true;
                       
                    if(gEnd == false){
                        addTile();
                    }             
                }
                
                //rozhodnutí dalšího postupu
                if(gEnd == true){
                    gEnd = false;
                    
                    stat = 2;
                    
                    rewrite_all = true;
                    rewrite_game = true;
                }  
                else if(gWin == true){
                    gWin = false;
                    
                    stat = 3;
                    
                    rewrite_all = true;
                    rewrite_game = true;
                }
                
                rewrite_game = true;    
            }
            
            //dolní část herní plochy s tlačítky
            else if(p.y > 240 && p.y < 310){
                
                //tlačítko SAVE
                if(p.x > 10 && p.x < 115){
                    Tft.fillRectangle(20,95,200,100, barvy[17]);
                    
                    //ukládání
                    if(SD.exists("G2048.TXT")){
                        SD.remove("G2048.TXT"); //vyčistíme případný starý soubor
                    }
                    soubor = SD.open("G2048.TXT", FILE_WRITE);
                        
                    //pokud se otevření povedlo, zapíšeme hodnoty
                    if(soubor){
                        for(int y = 0; y <=3; y++){
                            for(int x = 0; x <= 3; x++){
                                soubor.write(plocha[y][x]);   
                            }
                        }
                        soubor.close();
                        Tft.drawString("SAVED", 28, 123, 6, BLACK);
                    }
                    else{
                        Tft.drawString("ERROR", 28, 123, 6, BLACK);
                    }
                        
                    delay(1000);
                    
                    rewrite_all = true;
                    rewrite_game = true;
                } 
                
                //tlačítko MAIN MENU
                else if(p.x > 125 && p.x < 210){
                    stat = 0;                    
                    rewrite_all = true;           
                }              
            }
        }
    }
    
    //prohra
    else if(stat == 2){
        if(rewrite_all){
            Tft.fillRectangle(10,10,220,300,barvy[2]);
            Tft.fillRectangle(20,200,200,100, barvy[10]);
            
            Tft.drawString("GAME", 32, 35, 7, BLACK);
            Tft.drawString("OVER", 32, 115, 7, BLACK);
            Tft.drawString("MAIN MENU", 37, 238, 3, BLACK);
            
            rewrite_all = false;
        }
        
        checkP();
        
        //tlačítko MAIN MENU
        if(p.z > 10){
            if(p.x > 20 && p.x < 220 && p.y > 200 && p.y < 320){
                rewrite_all = true;
                stat = 0;
            }
        }
    }
    
    //výhra
    if(stat == 3){
        if(rewrite_all){
            Tft.fillRectangle(10,10,220,300,barvy[2]);
            Tft.fillRectangle(20,200,200,100, barvy[10]);
            
            Tft.drawString("YOU", 50, 35, 7, BLACK);
            Tft.drawString("WON", 50, 115, 7, BLACK);
            Tft.drawString("MAIN MENU", 37, 238, 3, BLACK);
            
            rewrite_all = false;
        }
        
        checkP();
        
        //tlačítko MAIN MENU
        if(p.z > 10){
            if(p.x > 20 && p.x < 220 && p.y > 200 && p.y < 320){
                rewrite_all = true;
                stat = 0;
            }
        }       
    }
}

//nastaví aktuální pozici dotyku uživatele
void checkP(){ 
    p = ts.getPoint();
    p.x = map(p.x, min_x, max_x, 0, 240);
    p.y = map(p.y, min_y, max_y, 0, 320);
}

//přidá novou dlaždici
void addTile(){ 
    byte r1, r2;
    
    do{
        r1 = random(4);
        r2 = random(4);      
    }while(plocha[r1][r2] != 0);
                
    plocha[r1][r2] = random(1,3);    
}

void goUp(){
    for(int x = 0; x <= 3; x++){
        if(plocha[0][x] == plocha[1][x] && plocha[2][x] == plocha[3][x]){
            max_added = 2;       
        }
        else{
            max_added = 1;
        }
                        
        for(int y = 0; y <= 2; y++){
            for(int y_p = y; y_p >= 0; y_p--){
                if(plocha[y_p][x] == plocha[y_p+1][x] && plocha[y_p][x] != 0 && added < max_added){
                    plocha[y_p][x]++;
                    plocha[y_p+1][x] = 0;
                    added++;
                    moved = true;
                }
                if(plocha[y_p][x] == 0 && plocha[y_p+1][x] != 0){
                    plocha[y_p][x] = plocha[y_p+1][x];
                    plocha[y_p+1][x] = 0;
                    moved = true;
                }
            }     
        }
        added = 0;    
    }
}

void goDown(){
    for(int x = 0; x <= 3; x++){
        if(plocha[0][x] == plocha[1][x] && plocha[2][x] == plocha[3][x]){
            max_added = 2;       
        }
        else{
            max_added = 1;
        }
                            
        for(int y = 3; y >= 1; y--){
            for(int y_p = y; y_p <= 3; y_p++){
                if(plocha[y_p-1][x] == plocha[y_p][x] && plocha[y_p][x] != 0 && added < max_added){
                    plocha[y_p-1][x]++;
                    plocha[y_p][x] = 0; 
                    added++;
                    moved = true;
                }
                if(plocha[y_p][x] == 0 && plocha[y_p-1][x] != 0){
                    plocha[y_p][x] = plocha[y_p-1][x];
                    plocha[y_p-1][x] = 0;
                    moved = true;
                }
            }    
        }
        added = 0;     
    }
}

void goRight(){
    for(int y = 0; y <= 3; y++){
        if(plocha[y][0] == plocha[y][1] && plocha[y][2] == plocha[y][3]){
            max_added = 2;       
        }
        else{
            max_added = 1;
        }
                            
        for(int x = 3; x >= 1; x--){
            for(int x_p = x; x_p <= 3; x_p++){
                if(plocha[y][x_p-1] == plocha[y][x_p] && plocha[y][x_p] != 0 && added < max_added){
                    plocha[y][x_p-1]++;
                    plocha[y][x_p] = 0;
                    added++;
                    moved = true;
                }
                if(plocha[y][x_p] == 0 && plocha[y][x_p-1] != 0){
                    plocha[y][x_p] = plocha[y][x_p-1];
                    plocha[y][x_p-1] = 0;
                    moved = true;
                }
            }    
        }
        added = 0;     
    }
}

void goLeft(){
    for(int y = 0; y <= 3; y++){
        if(plocha[y][0] == plocha[y][1] && plocha[y][2] == plocha[y][3]){
            max_added = 2;       
        }
        else{
            max_added = 1;
        }
                            
        for(int x = 0; x <= 2; x++){
            for(int x_p = x; x_p >= 0; x_p--){
                if(plocha[y][x_p] == plocha[y][x_p+1] && plocha[y][x_p] != 0 && added < max_added){
                    plocha[y][x_p]++;
                    plocha[y][x_p+1] = 0;  
                    added++;
                    moved = true;
                }
                if(plocha[y][x_p] == 0 && plocha[y][x_p+1]){
                    plocha[y][x_p] = plocha[y][x_p+1];
                    plocha[y][x_p+1] = 0;
                    moved = true;
                }
            }     
        }
        added = 0;     
    }     
}
