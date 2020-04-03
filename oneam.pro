QT += core
QT += gui
QT += widgets

win32 {
   QTPLUGIN += plugins/qjpeg4
}

TARGET = oneam
TEMPLATE = app

MOC_DIR = .moc
UI_DIR = .ui
OBJECTS_DIR = .obj

INCLUDEPATH += thirdparty/bit7z

INCLUDEPATH += src

LIBS += -L$${PWD}/lib
LIBS += -loleaut32
LIBS += -luser32

CONFIG(release, debug|release) {
   LIBS += -lbit7z64
}
CONFIG(debug, debug|release) {
   LIBS += -lbit7z64_d
}

SOURCES += src/main.cpp\
    src/unpacker.cpp \
    src/previewwidget.cpp \
    src/pagewidget.cpp \
    src/book.cpp \
    src/mainwidget.cpp \
    src/comicbookitem.cpp \
    src/comicconstants.cpp \
    src/configwidget.cpp \
    src/config.cpp

HEADERS  += \
    thirdparty/bit7z/bit7z.hpp \
    thirdparty/bit7z/bit7zlibrary.hpp \
    thirdparty/bit7z/bitarchivecreator.hpp \
    thirdparty/bit7z/bitarchivehandler.hpp \
    thirdparty/bit7z/bitarchiveinfo.hpp \
    thirdparty/bit7z/bitarchiveitem.hpp \
    thirdparty/bit7z/bitarchiveopener.hpp \
    thirdparty/bit7z/bitcompressionlevel.hpp \
    thirdparty/bit7z/bitcompressionmethod.hpp \
    thirdparty/bit7z/bitcompressor.hpp \
    thirdparty/bit7z/bitexception.hpp \
    thirdparty/bit7z/bitextractor.hpp \
    thirdparty/bit7z/bitformat.hpp \
    thirdparty/bit7z/bitguids.hpp \
    thirdparty/bit7z/bitinputarchive.hpp \
    thirdparty/bit7z/bitmemcompressor.hpp \
    thirdparty/bit7z/bitmemextractor.hpp \
    thirdparty/bit7z/bitpropvariant.hpp \
    thirdparty/bit7z/bitstreamcompressor.hpp \
    thirdparty/bit7z/bitstreamextractor.hpp \
    thirdparty/bit7z/bittypes.hpp \
    src/unpacker.h \
    src/previewwidget.h \
    src/pagewidget.h \
    src/book.h \
    src/mainwidget.h \
    src/comicbookitem.h \
    src/comicconstants.h \
    src/configwidget.h \
    src/config.h

FORMS += \
    src/previewwidget.ui \
    src/pagewidget.ui \
    src/mainwidget.ui \
    src/configwidget.ui

OTHER_FILES += \
    oneam.ini

