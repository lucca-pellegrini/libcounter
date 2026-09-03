# libcounter

Protótipo de contador de pessoas para a biblioteca do prédio 4 da PUC Minas, Unidade Lourdes. Substitui um contador comercial a laser (que parou de funcionar) por um firmware em **ESP32-S3** e **ESP-IDF**, baseado em sensor ultrassônico.

O número exibido corresponde à metade do total de detecções (`count >> 1`): como há uma única entrada/saída, quem entra também sai por ali, então o sensor conta cada pessoa duas vezes. Ao final do dia, o pessoal da biblioteca apenas anota o valor do display.

## Hardware

| Componente | Detalhe |
|---|---|
| MCU | ESP32-S3 (N16R8: 16 MB flash, 8 MB PSRAM octal) |
| Sensor | Ultrassônico tipo HC-SR04 / AJ-SR04M / JSN-SR04T (TRIG GPIO17, ECHO GPIO16) |
| Display | OLED SSD1306 128×64, I2C (SCL GPIO9, SDA GPIO8) |
| LED | RGB WS2812/NeoPixel interno do DevKit (GPIO48) |
| Botão | Pushbutton ativo-alto com pull-down (GPIO5) |

## Dependências

Projeto para **ESP-IDF 6.1.0** (submódulo). Componentes externos (via Component Manager):

- `k0i05/esp_ssd1306` — driver do display OLED
- `espressif/led_strip` — driver do LED RGB via RMT

## Funcionamento

- **Contagem**: uma tarefa de alta prioridade mede a distância periodicamente (50 ms). Uma média móvel de 3 leituras suaviza ruído. Uma pessoa é contada somente após `CONFIRM_READS` (2) leituras consecutivas abaixo do limiar (50 cm), e o próximo objeto só é contado após `COOLDOWN_READS` (10) leituras limpas consecutivas. O contador interno incrementa de 2 em 2; o bit 0 marca alguém passando no momento (quadradinho no canto superior do display).
- **Persistência (NVS)**: o valor é salvo automaticamente a cada `NVS_SAVE_INTERVAL_SECONDS` (300 s), pulando a escrita quando inalterado. Isso reduz o desgaste do NVS mantendo, no máximo, uma hora (em uma passagem) de perda em caso de queda de energia.
- **Botão**:
  - Toque curto inicia fluxo de **zerar** o contador para o dia seguinte; confirme na tela com um segundo toque dentro de 5 s.
  - Pressionar e segurar (≥ 500 ms) **salva manualmente** no NVS.
- **LED**:
  - Laranja: alguém/algo na área de leitura do sensor (bloqueando-o), mas já contado. O sistema ainda não está *armado* para a próxima contagem, pois o objeto ainda não saiu do caminho.
  - Apagado (ocioso): nenhum objeto detectado.
  - Azul: flash ao contar uma pessoa.
  - Verde piscando durante o salvamento; vermelho piscando na confirmação de reset; vermelho fixo por 2 s após zerar.
- **Modo debug**: segurar o botão ao ligar pula a animação, ignora o valor salvo e carrega um valor fixo (`CONFIG_DEBUG_COUNT`), exibindo também a distância medida.
- **Boot**: animação inicial que pode ser pulada com um toque no botão.

## Estados da máquina

| Estado | Descrição |
|---|---|
| `RUNNING` | Operação normal. Sensor, display e LED ativos |
| `PAUSED` | Confirmação de reset em andamento (sensor para de contar) |
| `SAVING` | Animação de salvamento. O sensor **continua contando** para não perder passagens |

## Tarefas FreeRTOS

| Tarefa | Prioridade | Função |
|---|---|---|
| `sensor_task` | 10 | Medição e lógica de detecção |
| `button_task` | 5 | Classificação de toque e ações |
| `display_task` | 5 | Atualização periódica do display |
| `nvs_save_task` | 3 | Persistência periódica em NVS |
| `boot_anim_task` | 4 | Animação de inicialização (criada e encerrada) |

## Configuração

Tudo é ajustável via menuconfig em *Library Counter Configuration*: pinos GPIO, limiar de detecção, intervalos, quantidade de leituras de confirmação/cooldown, tempo de salvamento NVS e valor de debug.

## Build

Requer ESP-IDF fonte e `idf.py` no PATH.

```sh
idf.py set-target esp32s3
idf.py build flash monitor
```

## Licença

Licenciado sob licensa ISC. Veja o arquivo [LICENSE](LICENSE) para detalhes.
