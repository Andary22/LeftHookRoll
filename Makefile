CXX = c++
CXXFLAGS = -Wall -Wextra -Werror -std=c++98 -Wshadow -MMD -MP -Iincludes

SRCDIR = srcs
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

-include $(DEPS)

.PHONY: all clean fclean re