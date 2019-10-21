/* ********************************************************** *
 * Projeto Medidor de Irradiação                              *
 *                                                            *
 * O medidor segue a linha que define o círculo de irradiação *
 * e pára em pontos específicos para realizar a medição.      *
 * Esses pontos são definidos por faixas retas que obstruem   *
 * ambos sensores infravermelhos.                             *
 *                                                            *
 * Atenção:                                                   *
 * O sentido preferencial para o medidor rodar é HORÁRIO.     *
 *                                                            *
 * Projeto feito por:                                         *
 *  Alex Junio                                                *
 *  Érika Tagima                                              *
 *  Gabriel Lobão                                             *
 *  Ryann Santos                                              *
 *                                                            *
 * Agradecimento especial por todo auxílio:                   *
 *  Gustavo Lopes                                             *
 *                                                            *
 * ********************************************************** */

#include <SPI.h>  // Comunicação com SD Card
#include <SD.h>   // Gerenciamento do SD Card

// =================================================== //
//                 DECLARAÇÃO DE PINOS                 //
// =================================================== //

// Sensores infravermelho
#define Sensor_direita  2
#define Sensor_esquerda 3

// Ponte H
#define MotorA_H    8  // Sentido Horário
#define MotorA_A    7  // Sentido Anti horário
#define MotorB_H    6
#define MotorB_A    4
#define MotorA_PWM  5
#define MotorB_PWM  9

// SD CARD
#define Chip_select 10

// Medidor
#define Medidor     A0

// Botão de start
#define Botao       A1

// =================================================== //
//                      CONSTANTES                     //
// =================================================== //

// Velocidades default
#define VELOCIDADE_0  0
#define VELOCIDADE_1  130 // Velocidade para virar à direita (1)
#define VELOCIDADE_2  145 // Velocidade para virar à direita (2)
#define VELOCIDADE_3  150 // Velocidade para tirar da inércia
#define VELOCIDADE_4  60
#define VELOCIDADE_5  65
#define VELOCIDADE_6  130  // Velocidade para virar à esquerda (1)
#define VELOCIDADE_7  145 // Velocidade para virar à esquerda (1)
#define VELOCIDADE_8  150 // Velocidade para ir para frente
#define DIREITO_MUL   1.0
#define ESQUERDO_MUL  1.2

// Constantes do controle
#define KP 1
#define KI 1

// Número de pontos analisados
// Exemplo:
//  Se a medição for feita em um círculo completo, são 360°.
//  Se forem feitas medidas a cada 10°:
//    360° / 10° = 36 pontos.
#define QUANTIDADE_PONTOS 16

// =================================================== //
//                      VARIÁVEIS                      //
// =================================================== //

// Leitura dos sensores
int direita, esquerda;
int medidor_input;

// Atuação dos motores
int direita_out, esquerda_out;

// Variáveis do cartão SD
File root;
File entry;
String entry_name;
File arquivo_medidas;

// Variáveis de controle PI
unsigned long tempoAtual = 0, tempoAnterior = 0;
double tempoDecorrido = 0;
double erroAcumulado = 0;

// Entrada da serial
char received = 0;
int incomingByte = 0;

// Contador de pontos de medição
int pontos_medidos = 0;

// =================================================== //
//                       PROGRAMA                      //
// =================================================== //

void setup() {
  Serial.begin(9600);

  pinMode(Sensor_direita, INPUT);
  pinMode(Sensor_esquerda, INPUT);
  
  pinMode(MotorA_H, OUTPUT);
  pinMode(MotorA_A, OUTPUT);
  pinMode(MotorB_H, OUTPUT);
  pinMode(MotorB_A, OUTPUT);
  pinMode(MotorA_PWM, OUTPUT);
  pinMode(MotorB_PWM, OUTPUT);

  pinMode(Botao, INPUT_PULLUP);

  // Começa parado
  analogWrite(MotorA_PWM, 0);
  analogWrite(MotorB_PWM, 0);

  // Checkando o status do SD card
  Serial.println("Sis : Inicializando o SD Card...");

  if (!SD.begin(Chip_select))
  {
    Serial.println("Erro: Falha ao iniciar o SD Card.");
    Serial.println("Erro: Verifique se o cartão está inserido e reinicie o sistema.");
    return;
  }
  
  Serial.println("OK  : SD Card pronto.");
}

void loop() {

  received = Serial.read();

  if (received == 'C')
  {
    Serial.println("Sis : Calibracao do log detector...");
    while (1)
    {
      medidor_input = analogRead(Medidor);
      Serial.println(medidor_input);
      delay(500);
    }
  }

  if (received == 'S' || !digitalRead(Botao))  // Comando de início
  {
    Serial.println("Sis : Medicao da antena...");

    // Seta o motor para andar para frente
    digitalWrite(MotorA_H, LOW);
    digitalWrite(MotorA_A, HIGH);
    digitalWrite(MotorB_H, HIGH);
    digitalWrite(MotorB_A, LOW);

    // Criando um novo arquivo de medidas (numerando de acordo)
    int file_number = 0;
    root = SD.open("/");

    while(1)
    {
      entry = root.openNextFile();
      
      if(!entry)
        break;

      entry_name = entry.name();

      if(entry_name.substring(0,4) == "MED_") // Verificando se é um arquivo "medida"
      {
        if(entry_name.substring(4,6).toInt() > file_number)
        {
          file_number = entry_name.substring(4,6).toInt();  // Pegando última numeração
        }
      }
    }

    // Aqui já temos o número do arquivo que deve ser gerado
    // Para que tudo fique organizado dentro da pasta, vamos numerar no formato
    // 00, 01, 02, ... , 98, 99.
    file_number++;
    Serial.print("Sis : Criando medidas ");
    Serial.print(file_number);
    Serial.println("...");

    entry_name = "med_";
    
    if(file_number < 9)
    {
      entry_name += 0;
    }

    entry_name += file_number;
        
    arquivo_medidas = SD.open(entry_name, FILE_WRITE);
    arquivo_medidas.print("Dados de irradiacao ");
    arquivo_medidas.println(file_number);
    arquivo_medidas.close();
    
    if(!SD.exists(entry_name))  // Se o arquivo não foi criado, não tem muito o que fazer.
    {
      Serial.print("Erro: O arquivo ");
      Serial.print(entry_name);
      Serial.println(" nao foi criado corretamente.");
      return;
    }

    Serial.println("Sis : Iniciando medicao...\n");
    arquivo_medidas = SD.open(entry_name, FILE_WRITE);  // Abrindo novamente o arquivo
    arquivo_medidas.println("ponto_medido,valor_dBm");

    // Tirando o carro da inércia
    analogWrite(MotorA_PWM, DIREITO_MUL*VELOCIDADE_3);
    analogWrite(MotorB_PWM, ESQUERDO_MUL*VELOCIDADE_3);

    delay(300);

    // Loop de medição
    while (1)
    {
      if(Serial.available())
      {
        incomingByte = Serial.read();

        if(incomingByte == 'P')
        {
          analogWrite(MotorA_PWM, DIREITO_MUL*VELOCIDADE_0);
          analogWrite(MotorB_PWM, ESQUERDO_MUL*VELOCIDADE_0);
          while(1);
        }
      }
      
      // Leituras dos Sensores
      direita = digitalRead(Sensor_direita);
      esquerda = digitalRead(Sensor_esquerda);
      
      Serial.print("Sis : Sensores - ");
      Serial.print(esquerda);
      Serial.print(" || ");
      Serial.println(direita);
      
      // Controle do carro
      if(direita == false && esquerda == false)
      {
        digitalWrite(MotorA_H, LOW);
        digitalWrite(MotorA_A, HIGH);
        digitalWrite(MotorB_H, HIGH);
        digitalWrite(MotorB_A, LOW);
        
        analogWrite(MotorA_PWM, DIREITO_MUL*VELOCIDADE_8);
        analogWrite(MotorB_PWM, ESQUERDO_MUL*VELOCIDADE_8);
      }

      if(direita == false && esquerda == true)
      {
        digitalWrite(MotorA_H, HIGH);
        digitalWrite(MotorA_A, LOW);
        digitalWrite(MotorB_H, HIGH);
        digitalWrite(MotorB_A, LOW);
        
        analogWrite(MotorA_PWM, DIREITO_MUL*VELOCIDADE_1);
        analogWrite(MotorB_PWM, ESQUERDO_MUL*VELOCIDADE_2);
      }

      if(direita == true && esquerda == false)
      {
        digitalWrite(MotorA_H, LOW);
        digitalWrite(MotorA_A, HIGH);
        digitalWrite(MotorB_H, LOW);
        digitalWrite(MotorB_A, HIGH);
        
        analogWrite(MotorA_PWM, DIREITO_MUL*VELOCIDADE_7);
        analogWrite(MotorB_PWM, ESQUERDO_MUL*VELOCIDADE_6);
      }

      if(direita == true && esquerda == true)   // O carrinho deve parar pois tem um risco de medição na pista
      {
        //analogWrite(MotorA_PWM, VELOCIDADE_0);
        //analogWrite(MotorB_PWM, VELOCIDADE_0);

        //delay(50);
        
        // Seta o motor para andar para trás (solavanco)
        digitalWrite(MotorA_H, HIGH);
        digitalWrite(MotorA_A, LOW);
        digitalWrite(MotorB_H, LOW);
        digitalWrite(MotorB_A, HIGH);

        analogWrite(MotorA_PWM, DIREITO_MUL*VELOCIDADE_2);
        analogWrite(MotorB_PWM, ESQUERDO_MUL*VELOCIDADE_2);
        
        delay(50);

        analogWrite(MotorA_PWM, DIREITO_MUL*VELOCIDADE_0);
        analogWrite(MotorB_PWM, ESQUERDO_MUL*VELOCIDADE_0);

        lerMedidor();
        gravarDados(convAnalogPot());
        proximoPonto();
      }

      // Critério de parada
      if(pontos_medidos >= QUANTIDADE_PONTOS)   // Passou por todos os pontos
      {
        analogWrite(MotorA_PWM, DIREITO_MUL*VELOCIDADE_0);  // Garantindo que o carro vai ficar parado
        analogWrite(MotorB_PWM, ESQUERDO_MUL*VELOCIDADE_0);
        arquivo_medidas.close();                // Fechando o arquivo de medidas
        
        while(1);                               // Travando a execução
      }

      for(int j = 0; j < 5; j++)
      {
        // Leituras dos Sensores
        direita = digitalRead(Sensor_direita);
        esquerda = digitalRead(Sensor_esquerda);
  
        if(direita == true && esquerda == true)
        {
          analogWrite(MotorA_PWM, DIREITO_MUL*VELOCIDADE_0);
          analogWrite(MotorB_PWM, ESQUERDO_MUL*VELOCIDADE_0);
          
        }
        
        delay(10);
      }

      analogWrite(MotorA_PWM, DIREITO_MUL*VELOCIDADE_0);
      analogWrite(MotorB_PWM, ESQUERDO_MUL*VELOCIDADE_0);

      delay(50);
    }
  }
}

// =================================================== //
//                       FUNÇÕES                       //
// =================================================== //

/* Ler Medidor
 * Lê dados da porta analógica referente à saída do medidor.
 */
void lerMedidor() {

  delay(500);

  medidor_input = analogRead(Medidor);
  
  Serial.print("Sis : Medidor - ");
  Serial.println(medidor_input);
}

/* Converter entrada Analógica para Potência (dBm)
 * Com o dado recebido do medidor, cria-se uma relação da entrada com a potência da 
 * antena irradiante.
 * Considerando que o Arduino foi calibrado, essa relação é:
 *  -60dBm = 750
 *  -10dBm = 50
 * De acordo com o gráfico encontrado pelo Ryann, essa relação pode ser extendida para uma 
 * função de 1° grau com y = Ax + B.
 * Portanto:
 *  y = (5/70)*x + (45/7)
 * (Nessa equação, y vai sair positivo. Como é uma perda (-k dBm), troque o sinal)
 */
float convAnalogPot() {

  return (5.0/70.0)*(float)medidor_input + (45.0/7.0);
}

/* Gravar Dados no cartão SD
 * Pega a leitura em dBm e grava de acordo com o ponto analisado
 */
void gravarDados(float d) {

  Serial.print("Sis : Gravando ");
  Serial.print(pontos_medidos);
  Serial.print(",");
  Serial.println(d);

  arquivo_medidas.print(pontos_medidos);
  arquivo_medidas.print(",");
  arquivo_medidas.println(d);

  delay(500);
}

/* passar para o Próximo Ponto
 * Lê dados da porta analógica referente à saída do medidor.
 * Como o sentido é sempre horário, ele vai tender para esse lado.
 */
void proximoPonto() {
  
  // Seta o motor para andar para frente
  digitalWrite(MotorA_H, LOW);
  digitalWrite(MotorA_A, HIGH);
  digitalWrite(MotorB_H, HIGH);
  digitalWrite(MotorB_A, LOW);

  // Tirando o carro da inércia
  analogWrite(MotorA_PWM, DIREITO_MUL*VELOCIDADE_3);
  analogWrite(MotorB_PWM, ESQUERDO_MUL*VELOCIDADE_3);
  
  delay(200);

  // Procurando faixa
  while(direita == true && esquerda == true)
  {
    if(Serial.available())
    {
      incomingByte = Serial.read();

      if(incomingByte == 'P')
      {
        analogWrite(MotorA_PWM, DIREITO_MUL*VELOCIDADE_0);
        analogWrite(MotorB_PWM, ESQUERDO_MUL*VELOCIDADE_0);
        while(1);
      }
    }
    
    // Leituras dos Sensores
    direita = digitalRead(Sensor_direita);
    esquerda = digitalRead(Sensor_esquerda);
    
    digitalWrite(MotorA_H, LOW);
    digitalWrite(MotorA_A, HIGH);
    digitalWrite(MotorB_H, LOW);
    digitalWrite(MotorB_A, HIGH);
    
    analogWrite(MotorA_PWM, DIREITO_MUL*VELOCIDADE_5);
    analogWrite(MotorB_PWM, ESQUERDO_MUL*VELOCIDADE_4);
    
    delay(50);
  }
  
  pontos_medidos++;
}


