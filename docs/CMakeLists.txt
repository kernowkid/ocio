
###############################################################################
### External Doc Apps ###

# setuptools
set(SETUPTOOLS_VERSION 0.6c11)
ExternalProject_Add(setuptools
    URL ${CMAKE_SOURCE_DIR}/ext/setuptools-${SETUPTOOLS_VERSION}.tar.gz
    BUILD_IN_SOURCE 1
    CONFIGURE_COMMAND ${CMAKE_COMMAND} -E make_directory ${EXTDIST_PYTHONPATH}
    BUILD_COMMAND PYTHONPATH=${PYTHONPATH} python setup.py build
    INSTALL_COMMAND PYTHONPATH=${PYTHONPATH} python setup.py install --prefix=${EXTDIST_ROOT}
)

# docutils
set(DOCUTILS_VERSION 0.7)
ExternalProject_Add(docutils
    DEPENDS setuptools
    URL ${CMAKE_SOURCE_DIR}/ext/docutils-${DOCUTILS_VERSION}.tar.gz
    BUILD_IN_SOURCE 1
    CONFIGURE_COMMAND ${CMAKE_COMMAND} -E make_directory ${EXTDIST_PYTHONPATH}
    BUILD_COMMAND PYTHONPATH=${PYTHONPATH} python setup.py build
    INSTALL_COMMAND PYTHONPATH=${PYTHONPATH} python setup.py install --prefix=${EXTDIST_ROOT}
)

# jinja2
set(JINJA2_VERSION 2.5.5)
ExternalProject_Add(Jinja2
    DEPENDS setuptools
    URL ${CMAKE_SOURCE_DIR}/ext/Jinja2-${JINJA2_VERSION}.tar.gz
    BUILD_IN_SOURCE 1
    CONFIGURE_COMMAND ${CMAKE_COMMAND} -E make_directory ${EXTDIST_PYTHONPATH}
    BUILD_COMMAND PYTHONPATH=${PYTHONPATH} python setup.py build
    INSTALL_COMMAND PYTHONPATH=${PYTHONPATH} python setup.py install --prefix=${EXTDIST_ROOT}
)

# Pygments
set(PYGMENTS_VERSION 1.3.1)
ExternalProject_Add(Pygments
    DEPENDS setuptools
    URL ${CMAKE_SOURCE_DIR}/ext/Pygments-${PYGMENTS_VERSION}.tar.gz
    BUILD_IN_SOURCE 1
    CONFIGURE_COMMAND ${CMAKE_COMMAND} -E make_directory ${EXTDIST_PYTHONPATH}
    BUILD_COMMAND PYTHONPATH=${PYTHONPATH} python setup.py build
    INSTALL_COMMAND PYTHONPATH=${PYTHONPATH} python setup.py install --prefix=${EXTDIST_ROOT}
)

# sphinx
set(SPHINX_VERSION f52b631f501a) # 1.1pre
ExternalProject_Add(Sphinx
    DEPENDS setuptools docutils Jinja2 Pygments
    URL ${CMAKE_SOURCE_DIR}/ext/sphinx-${SPHINX_VERSION}.tar.gz
    PATCH_COMMAND patch -p1 < ${CMAKE_SOURCE_DIR}/ext/sphinx-${SPHINX_VERSION}.patch
    BUILD_IN_SOURCE 1
    CONFIGURE_COMMAND ${CMAKE_COMMAND} -E make_directory ${EXTDIST_PYTHONPATH}
    BUILD_COMMAND PYTHONPATH=${PYTHONPATH} python setup.py build
    INSTALL_COMMAND PYTHONPATH=${PYTHONPATH} python setup.py install --prefix=${EXTDIST_ROOT}
)

###############################################################################
### Create Doc Targets ###

message(STATUS "Create sphinx conf.py from conf.py.in")
configure_file(${CMAKE_SOURCE_DIR}/docs/conf.py.in
    ${CMAKE_BINARY_DIR}/docs/conf.py @ONLY)

message(STATUS "Copy doc files to staging area")
file(GLOB_RECURSE DOCFILES *.rst)
list(APPEND DOCFILES ${CMAKE_SOURCE_DIR}/README)
list(APPEND DOCFILES ${CMAKE_SOURCE_DIR}/INSTALL)
list(APPEND DOCFILES ${CMAKE_SOURCE_DIR}/ChangeLog)
list(APPEND DOCFILES ${CMAKE_SOURCE_DIR}/LICENSE)

CopyFiles(RSTDOC ${DOCFILES})

ExtractRst(${CMAKE_SOURCE_DIR}/export/OpenColorIO/OpenColorIO.h OpenColorIO.rst)
ExtractRst(${CMAKE_SOURCE_DIR}/export/OpenColorIO/OpenColorTransforms.h OpenColorTransforms.rst)
ExtractRst(${CMAKE_SOURCE_DIR}/export/OpenColorIO/OpenColorTypes.h OpenColorTypes.rst)

add_custom_target(doc ALL
    COMMAND PYTHONPATH=${PYTHONPATH} ${EXTDIST_BINPATH}/sphinx-build -b html . ${CMAKE_CURRENT_BINARY_DIR}/build-html
    DEPENDS
        ${CMAKE_BINARY_DIR}/docs/conf.py
        OpenColorIO.rst
        OpenColorTransforms.rst
        OpenColorTypes.rst
        ${RSTDOC_OUTPUT}
    COMMENT "Building html docs"
    SOURCES ${DOCFILES})
# note: ExternalProject will not build when added to a add_custom_target this
# works around this problem. This seems to be fixed in the cmake ^HEAD
add_dependencies(doc Sphinx) 

install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/build-html/
        DESTINATION ${CMAKE_INSTALL_PREFIX}/share/OpenColorIO/docs/html
        PATTERN .git EXCLUDE)

find_package(LATEX)
if(PDFLATEX_COMPILER)
    
    add_custom_target(latex
        COMMAND PYTHONPATH=${PYTHONPATH} ${EXTDIST_BINPATH}/sphinx-build -b latex . ${CMAKE_CURRENT_BINARY_DIR}/build-latex
        DEPENDS
            ${CMAKE_BINARY_DIR}/docs/conf.py
            OpenColorIO.rst
            OpenColorTransforms.rst
            OpenColorTypes.rst
            ${RSTDOC_OUTPUT}
        COMMENT "Building latex doc"
        SOURCES ${DOCFILES})
    add_dependencies(latex Sphinx)
    
    add_custom_target(pdf ALL
        COMMAND ${PDFLATEX_COMPILER} OpenColorIO.tex
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/build-latex
        COMMENT "Building pdf doc"
        SOURCES ${DOCFILES})
    add_dependencies(pdf latex)
    
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/build-latex/OpenColorIO.pdf
            DESTINATION ${CMAKE_INSTALL_PREFIX}/share/OpenColorIO/docs/)
    
endif()