#include <stdio.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_task_wdt.h"

#define STUDENT_INFO "{Hugo Leonardo-RM:89360}"

#define WDT_TIMEOUT_S 6
#define QUEUE_LENGTH 10
#define QUEUE_ITEM_SIZE sizeof(int)
#define GENERATION_DELAY_MS 500
#define RECEPTION_TIMEOUT_MS 3000
#define SUPERVISION_DELAY_MS 2000

static QueueHandle_t dataQueue = NULL;

volatile bool generation_ok = false;
volatile bool reception_ok = false;

static const char *TAG = "PROVA_FINAL";

void init_task_watchdog(void)
{
    esp_task_wdt_config_t twdt_config = {
        .timeout_ms = WDT_TIMEOUT_S * 1000,
        .idle_core_mask = (1 << portNUM_PROCESSORS) - 1,
        .trigger_panic = true
    };
    esp_task_wdt_init(&twdt_config);
    ESP_LOGI(TAG, STUDENT_INFO " [WDT] Watchdog inicializado com timeout de %d segundos.", WDT_TIMEOUT_S);
}

void task_generate(void *pvParameters)
{
    esp_task_wdt_add(NULL);
    int counter = 0;
    while (1)
    {
        int value = counter++;
        if (xQueueSend(dataQueue, &value, 0) == pdPASS)
        {
            ESP_LOGI(TAG, STUDENT_INFO " [GERADOR] Valor %d enviado para a fila.", value);
        }
        else
        {
            ESP_LOGW(TAG, STUDENT_INFO " [GERADOR] Fila cheia, valor %d descartado.", value);
        }
        generation_ok = true;
        esp_task_wdt_reset();
        vTaskDelay(pdMS_TO_TICKS(GENERATION_DELAY_MS));
    }
}

void task_receive(void *pvParameters)
{
    esp_task_wdt_add(NULL);
    int receivedValue;
    while (1)
    {
        if (xQueueReceive(dataQueue, &receivedValue, pdMS_TO_TICKS(RECEPTION_TIMEOUT_MS)) == pdPASS)
        {
            int *ptr = (int *)malloc(sizeof(int));
            if (ptr != NULL)
            {
                *ptr = receivedValue;
                ESP_LOGI(TAG, STUDENT_INFO " [RECEPTOR] Valor %d recebido da fila.", *ptr);
                free(ptr);
            }
            else
            {
                ESP_LOGE(TAG, STUDENT_INFO " [RECEPTOR] ERRO: Falha ao alocar memória!");
            }
            reception_ok = true;
        }
        else
        {
            ESP_LOGW(TAG, STUDENT_INFO " [RECEPTOR] AVISO: Nenhum dado recebido no tempo limite.");
            ESP_LOGI(TAG, STUDENT_INFO " [RECEPTOR] Tentando recuperar...");
            vTaskDelay(pdMS_TO_TICKS(1000));

            if (xQueueReceive(dataQueue, &receivedValue, pdMS_TO_TICKS(1000)) == pdPASS)
            {
                ESP_LOGI(TAG, STUDENT_INFO " [RECEPTOR] Recuperação bem-sucedida! Valor: %d", receivedValue);
                reception_ok = true;
            }
            else
            {
                ESP_LOGE(TAG, STUDENT_INFO " [RECEPTOR] ERRO FATAL: Falha persistente. Encerrando tarefa...");
                reception_ok = false;
                vTaskDelete(NULL);
            }
        }
        esp_task_wdt_reset();
    }
}

void task_supervision(void *pvParameters)
{
    esp_task_wdt_add(NULL);
    while (1)
    {
        const char *gen_status_str = generation_ok ? "OK" : "FALHA";
        const char *recv_status_str = reception_ok ? "OK" : "FALHA";

        ESP_LOGI(TAG, STUDENT_INFO " [SUPERVISÃO] Status :: Geração=[%s] | Recepção=[%s] | Watchdog=[ATIVO]", gen_status_str, recv_status_str);

        generation_ok = false;
        reception_ok = false;

        esp_task_wdt_reset();
        vTaskDelay(pdMS_TO_TICKS(SUPERVISION_DELAY_MS));
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, STUDENT_INFO " [SISTEMA] Inicializando sistema multitarefa...");
    init_task_watchdog();
    dataQueue = xQueueCreate(QUEUE_LENGTH, QUEUE_ITEM_SIZE);
    if (dataQueue == NULL)
    {
        ESP_LOGE(TAG, STUDENT_INFO " [ERRO] Falha ao criar a fila!");
        return;
    }

    xTaskCreate(task_generate, "TaskGenerate", 2048, NULL, 2, NULL);
    xTaskCreate(task_receive, "TaskReceive", 4096, NULL, 2, NULL);
    xTaskCreate(task_supervision, "TaskSupervision", 4096, NULL, 1, NULL);

    ESP_LOGI(TAG, STUDENT_INFO " [SISTEMA] Tarefas iniciadas com sucesso!");
}
