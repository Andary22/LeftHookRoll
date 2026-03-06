CXX = c++
CXXFLAGS = -Wall -Wextra -Werror -std=c++98 -Wshadow -MMD -MP -Iincludes

SRCDIR = src
ODIR = ofiles

-include docs/sources.mk
SRC = $(addprefix $(SRCDIR)/,$(SRCS))
OBJS = $(addprefix $(ODIR)/,$(notdir $(SRC:.cpp=.o)))
DEPS = $(OBJS:.o=.d)

NAME = lefthookroll # forgetting to change this before submission will be funny

all: $(NAME)

$(NAME): $(OBJS)
	$(CXX) $(CXXFLAGS) $(OBJS) -o $(NAME)

$(ODIR)/%.o: $(SRCDIR)/%.cpp | $(ODIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(ODIR):
	mkdir -p $(ODIR)

clean:
	rm -rf $(ODIR)

fclean: clean
	rm -f $(NAME)

re: fclean all

TEST_RESPONSE_SRCS = tests/test_response.cpp \
	tests/stub_request.cpp \
	tests/stub_cgimgr.cpp \
	src/AllowedMethods.cpp \
	src/LocationConf.cpp \
	src/ServerConf.cpp \
	src/DataStore.cpp \
	src/FatalExceptions.cpp \
	src/Response.cpp

test_response: $(TEST_RESPONSE_SRCS)
	$(CXX) $(CXXFLAGS) $(TEST_RESPONSE_SRCS) -o tests/test_response
	./tests/test_response

-include $(DEPS)

.PHONY: all clean fclean re test_response