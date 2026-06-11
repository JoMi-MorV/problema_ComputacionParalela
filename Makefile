# Compilador y flags
CXX := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -Wpedantic -O2 -fopenmp
LDFLAGS := -lcurl -fopenmp

# Directorios
SRC_DIR := src
BUILD_DIR := build
OUTPUT_DIR := output

# Archivos fuente
SRCS := \
    $(SRC_DIR)/main.cpp \
    $(SRC_DIR)/utils/config.cpp \
    $(SRC_DIR)/utils/logger.cpp \
    $(SRC_DIR)/sftp/sftp_client.cpp \
    $(SRC_DIR)/csv/csv_parseo.cpp \
    $(SRC_DIR)/paralelismo/sftp_paralelo.cpp \
    $(SRC_DIR)/paralelismo/parseo_paralelo.cpp

# Headers
HEADERS := \
    $(SRC_DIR)/utils/config.h \
    $(SRC_DIR)/utils/logger.h \
    $(SRC_DIR)/sftp/sftp_client.h \
    $(SRC_DIR)/csv/csv_parseo.h \
    $(SRC_DIR)/csv/transaccion.h \
    $(SRC_DIR)/paralelismo/sftp_paralelo.h \
    $(SRC_DIR)/paralelismo/parseo_paralelo.h

# Objetos
OBJS := $(patsubst $(SRC_DIR)/%.cpp,$(BUILD_DIR)/%.o,$(SRCS))

# Ejecutable
TARGET := programa_paralela

.PHONY: all clean run dirs

all: dirs $(TARGET)

# Crear estructura de carpetas temporales de compilación
dirs:
	@mkdir -p $(BUILD_DIR)/utils
	@mkdir -p $(BUILD_DIR)/sftp
	@mkdir -p $(BUILD_DIR)/csv
	@mkdir -p $(BUILD_DIR)/paralelismo
	@mkdir -p $(OUTPUT_DIR)

# Link final
$(TARGET): $(OBJS)
	$(CXX) -o $@ $^ $(LDFLAGS)
	@echo "✓ Build completado"

# Compilar objetos individuales
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp $(HEADERS)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -I$(SRC_DIR) -c $< -o $@

# Ejecutar programa
run: all
	@./$(TARGET)

# Limpiar compilación previa
clean:
	rm -rf $(BUILD_DIR)
	rm -f $(TARGET)
	rm -f $(OUTPUT_DIR)/*.txt
	@echo "✓ Limpieza completada"
