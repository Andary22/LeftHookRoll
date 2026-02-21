# guidelines that will help us write cleaner code, and hopefully progress faster:

## naming conventions:
- class names should be in PascalCase(aka UpperCamelCase), Files containing class code will
always be named according to the class name.
- namespaces should be in snake_case, and the file containing the namespace should be named according to the namespace name.
- variables and functions should be in camelCase(aka lowerCamelCase).

## coding practices:
- **RAII.**
	- in short; RAII (Resource Acquisition Is Initialization) means that you should use constructors and destructors to manage resources, and avoid manual memory management as much as possible. (an object allocates a string in its constructor, and deallocates it in its destructor, so you don't have to worry about it)
- Use STL for data structures whenever possible.
- never swallow exceptions, and keep exception propagation minimal; handle the exception at the first boundary where you can actually do something about it.
- Optimization, follow best coding standards for example:
	- no unnecessary copies; pass by **const reference** when possible.
	- header separation (.hpp for declarations, .cpp for definitions)
	- free your functions when possible, and group them in namespaces.
		- a free function is function that is not a member of a class, it helps to:
			- increase encapsulation
			- keep classes small and focused
			- make code more testable.
- keep namespaces in the same class they help, unless said namespace is used by multiple classes, then it should be in a separate file. (like in the case of general utils)
- Document your code if need be, use doxygen style comments when applicable.
	- Documentation never justifies bad/unreadable code.
	- only document when it makes sense, and **avoid redundant ai slop documentation**; as a rule of thumb, comment's should explain the "why" behind the code, the "how/what" should be clear from the code itself.
- when a feature is added, add a test alongside it, and run all tests before merging to main/opening a PR.
- branch often, merge to main often; try to [follow this convention](https://www.conventionalcommits.org/en/v1.0.0/#summary) in commit messages, 1 commit per logical change, and PRs should be small and focused on a single change.
	- try to not merge to main directly, and always create a PR, even for small changes, this allows for code review and discussion, and greater understanding of the project for all of us.
- when you are the one reviewing a PR, hold it accountable to the guidelines.

Ramadan Kareem ;p