cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.1.0)
set(toolchainVersion 13_3_Rel1)
set(picotoolVersion 2.1.0)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
set(PICO_BOARD pico_w CACHE STRING "Board type")

# Importa o SDK do Raspberry Pi Pico
include(pico_sdk_import.cmake)

project(Ligeirinho C CXX ASM)

# Inicializa o SDK
pico_sdk_init()

# Adiciona o arquivo-fonte correto
add_executable(Ligeirinho Ligeirinho.c inc/ssd1306_i2c.c)

# Define o nome e a versão do programa
pico_set_program_name(Ligeirinho "Ligeirinho")
pico_set_program_version(Ligeirinho "0.1")

# Ativa saída USB para depuração
pico_enable_stdio_uart(Ligeirinho 0)
pico_enable_stdio_usb(Ligeirinho 1)

# Adiciona bibliotecas necessárias
target_link_libraries(Ligeirinho pico_stdlib hardware_timer hardware_pwm hardware_clocks hardware_i2c)

# Inclui diretórios do projeto
target_include_directories(Ligeirinho PRIVATE ${CMAKE_CURRENT_LIST_DIR})

# Gera arquivos adicionais necessários para o Pico
pico_add_extra_outputs(Ligeirinho)
