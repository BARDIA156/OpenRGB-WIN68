QT += core gui widgets
CONFIG += c++17 plugin
TEMPLATE = lib
TARGET = OpenRGB-WIN68

# Override at qmake invocation if the OpenRGB source is elsewhere.
isEmpty(OPENRGB_SOURCE): OPENRGB_SOURCE = $$PWD/../vendor/openrgb-1.0

INCLUDEPATH += \
    $$OPENRGB_SOURCE \
    $$OPENRGB_SOURCE/RGBController \
    $$OPENRGB_SOURCE/dependencies/json \
    $$OPENRGB_SOURCE/dependencies/hidapi-hotplug-win/include

SOURCES += \
    AulaHEDevice.cpp \
    AulaHEPlugin.cpp

HEADERS += \
    AulaHEDevice.h \
    AulaKeymap.h \
    AulaHEPlugin.h

win32:LIBS += -L$$OPENRGB_SOURCE/dependencies/hidapi-hotplug-win/x64 -lhidapi-hotplug
win32:QMAKE_CXXFLAGS += /utf-8

RESOURCES += openrgb-win68.qrc
