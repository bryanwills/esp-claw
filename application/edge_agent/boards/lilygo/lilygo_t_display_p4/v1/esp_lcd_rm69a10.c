/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "esp_check.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_rm69a10.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "esp_lcd_rm69a10";

#define RM69A10_CMD_WRDISBV    0x51

typedef struct {
    uint8_t cmd;
    const uint8_t *data;
    uint8_t data_len;
    uint16_t delay_ms;
} rm69a10_lcd_init_cmd_t;

static const rm69a10_lcd_init_cmd_t s_rm69a10_init_cmds[] = {
    {0xfe, (const uint8_t[]){0xfd}, sizeof((const uint8_t[]){0xfd}), 0},
    {0x80, (const uint8_t[]){0xfc}, sizeof((const uint8_t[]){0xfc}), 0},
    {0xfe, (const uint8_t[]){0x00}, sizeof((const uint8_t[]){0x00}), 0},
    {0x2a, (const uint8_t[]){0x00, 0x00, 0x02, 0x37}, sizeof((const uint8_t[]){0x00, 0x00, 0x02, 0x37}), 0},
    {0x2b, (const uint8_t[]){0x00, 0x00, 0x04, 0xcf}, sizeof((const uint8_t[]){0x00, 0x00, 0x04, 0xcf}), 0},
    {0x31, (const uint8_t[]){0x00, 0x03, 0x02, 0x34}, sizeof((const uint8_t[]){0x00, 0x03, 0x02, 0x34}), 0},
    {0x30, (const uint8_t[]){0x00, 0x00, 0x04, 0xcf}, sizeof((const uint8_t[]){0x00, 0x00, 0x04, 0xcf}), 0},
    {0x12, (const uint8_t[]){0x00}, sizeof((const uint8_t[]){0x00}), 0},
    {0x35, (const uint8_t[]){0x00}, sizeof((const uint8_t[]){0x00}), 0},
    {0x11, NULL, 0, 120},
    {0x29, NULL, 0, 0},
};

static esp_err_t rm69a10_check_id(esp_lcd_panel_io_handle_t io)
{
    uint8_t id = 0;
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_rx_param(io, 0xa1, &id, sizeof(id)),
                        TAG, "read id failed");
    ESP_RETURN_ON_FALSE(id == 0x01, ESP_FAIL, TAG, "unexpected id: 0x%02x", id);
    return ESP_OK;
}

static esp_err_t rm69a10_send_init_cmds(esp_lcd_panel_io_handle_t io)
{
    for (size_t i = 0; i < sizeof(s_rm69a10_init_cmds) / sizeof(s_rm69a10_init_cmds[0]); i++) {
        const rm69a10_lcd_init_cmd_t *cmd = &s_rm69a10_init_cmds[i];
        ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, cmd->cmd, cmd->data, cmd->data_len),
                            TAG, "send init command 0x%02x failed", cmd->cmd);
        if (cmd->delay_ms > 0) {
            vTaskDelay(pdMS_TO_TICKS(cmd->delay_ms));
        }
    }
    return ESP_OK;
}

esp_err_t esp_lcd_panel_rm69a10_set_brightness(esp_lcd_panel_io_handle_t io, uint8_t brightness)
{
    ESP_RETURN_ON_FALSE(io != NULL, ESP_ERR_INVALID_ARG, TAG, "invalid panel io");
    return esp_lcd_panel_io_tx_param(io, RM69A10_CMD_WRDISBV, &brightness, sizeof(brightness));
}

esp_err_t esp_lcd_new_panel_rm69a10(const esp_lcd_panel_io_handle_t io,
                                    const esp_lcd_panel_dev_config_t *panel_dev_config,
                                    esp_lcd_panel_handle_t *ret_panel)
{
    ESP_RETURN_ON_FALSE(io != NULL && panel_dev_config != NULL && ret_panel != NULL,
                        ESP_ERR_INVALID_ARG, TAG, "invalid arguments");
    const rm69a10_vendor_config_t *vendor = panel_dev_config->vendor_config;
    ESP_RETURN_ON_FALSE(vendor != NULL && vendor->dsi_bus != NULL && vendor->dpi_config != NULL,
                        ESP_ERR_INVALID_ARG, TAG, "invalid vendor config");

    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_dpi(vendor->dsi_bus, vendor->dpi_config, ret_panel),
                        TAG, "create DPI panel failed");
    ESP_RETURN_ON_ERROR(rm69a10_check_id(io), TAG, "check id failed");
    ESP_RETURN_ON_ERROR(rm69a10_send_init_cmds(io), TAG, "send init commands failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_rm69a10_set_brightness(io, 0xff),
                        TAG, "set default brightness failed");
    return ESP_OK;
}
