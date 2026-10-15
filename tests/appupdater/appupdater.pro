QT += core testlib widgets network
CONFIG += console testcase c++11
CONFIG -= app_bundle
TARGET = tst_appupdateinfo
SOURCES += tst_appupdateinfo.cpp ../../src/common/appupdater.cpp
HEADERS += ../../src/common/appupdater.h
