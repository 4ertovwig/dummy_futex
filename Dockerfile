FROM debian:10
MAINTAINER 4ertovwig <xperious@mail.ru>

ENV RUNLEVEL 1

#arguments
ARG WORKSPACE="/usr/src"
ARG PROJECT_NAME="dummy_futex"
ARG BIN_NAME="test_executable"

RUN set -eux; \
	\
	# prepare project
	mkdir ${WORKSPACE}/${PROJECT_NAME}; \
	mkdir ${WORKSPACE}/${PROJECT_NAME}/tests; \
	mkdir ${WORKSPACE}/${PROJECT_NAME}/include

RUN set -eux; \
	\
	# install environment
	apt-get update; \
	apt-get install -y cmake make g++ libboost-system-dev libgtest-dev; \
	apt-get clean && rm -rf /var/lib/apt/lists/* /var/tmp/*

RUN set -eux; \
	\
	# build googletest framework
	cd /usr/src/googletest/googletest; \
	mkdir build && cd build; \
	cmake .. && make -j$(getconf _NPROCESSORS_ONLN || echo 1) ; \
	cp libgtest* /usr/lib/ ; \
	cd - ; \
	rm -rf build; \
	mkdir /usr/local/lib/googletest; \
	ln -s /usr/lib/libgtest.a /usr/local/lib/googletest/libgtest.a; \
	ln -s /usr/lib/libgtest_main.a /usr/local/lib/googletest/libgtest_main.a

# copy sources
COPY include ${WORKSPACE}/${PROJECT_NAME}/include
COPY tests ${WORKSPACE}/${PROJECT_NAME}/tests
COPY CMakeLists.txt ${WORKSPACE}/${PROJECT_NAME}/

RUN set -eux; \
	\
	# build project
	mkdir ${WORKSPACE}/${PROJECT_NAME}/build; \
	cd ${WORKSPACE}/${PROJECT_NAME}/build; \
	cmake .. ;\
	make -j$(getconf _NPROCESSORS_ONLN || echo 1)

# run unit tests
ENTRYPOINT [ "/usr/src/dummy_futex/build/tests/test_executable" ]