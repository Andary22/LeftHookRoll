# How-to: Change the CGI interpreter to a different version

By default, the server looks for default interpreters in `/usr/bin/` and `/bin/`. If however you want to use a different version of the interpreters (such as a specific version of Python or Bash), you can modify the CGI interpreter path in the configuration file on a per-location basis.

1.  Open your configuration file.
2.  Locate the location block you want to tweak.
3.  Change the `cgi_interpreter` directive to point to your desired interpreter path.
4.  rerun the server with the updated configuration file.