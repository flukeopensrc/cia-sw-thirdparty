# Following are defaults for decnumber library.
# These defines must be defined when library clients are compiled.
# We don't recommend enabling them unless it's specifically required.
#DEFINES += DECNUMDIGITS=34  # default is 34 
#DEFINES += DECSUBSET=0      # default is 0
#DEFINES += DECEXTFLAG=1     # default is 1
#DEFINES += DECLITEND=0     # default is 1

CONFIG += debug


if(Q_OS_WIN32) {
 INCLUDEPATH += .
 # Remove Qt's defaults
 QMAKE_CXXFLAGS -= -Zc:wchar_t-
 # Add our defaults
 QMAKE_CXXFLAGS += /Zc:forScope /Zc:wchar_t
 DEFINES *= _CRT_SECURE_NO_WARNINGS
 # Are we in debug mode?
 debug {
   # Use Run-time checks for stack corruption and uninitialized var use
   QMAKE_CXXFLAGS += /RTC1
 }

} # end Q_OS_WIN32
else {
 MOC_DIR = moc
 OBJECTS_DIR = obj
}

