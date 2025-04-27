
#pragma once

#if defined(ESP_LOGV)
# undef ESP_LOGV
#endif
#define ESP_LOGV(l, fmt, ...) // Do nothing

#if defined(ESP_LOGD)
# undef ESP_LOGD
#endif
#define ESP_LOGD(l, fmt, ...) // Do nothing

#if defined(ESP_LOGI)
# undef ESP_LOGI
#endif
#define ESP_LOGI(l, fmt, ...) // Do nothing

#if defined(CONFIG_LOG_DISABLED)
# if defined(ESP_LOGW)
#  undef ESP_LOGW
# endif
# define ESP_LOGW(l, fmt, ...) // Do nothing
 
# if defined(ESP_LOGE)
#  undef ESP_LOGE
# endif
# define ESP_LOGE(l, fmt, ...) // Do nothing
#endif