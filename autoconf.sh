#!/bin/bash

generate_default() {
    local file=$1
    echo "Generating default configuration in $file..."
    echo -e "\n\r\n\r" >> "$file"
    cat << 'EOF' >> "$file"
server {
    listen 8080;
    server_name localhost;
    client_max_body_size 1M;

    location / {
        root /var/www/html;
        methods GET;
        autoindex on;
    }

    location /uploads {
        root /var/www;
        methods GET POST DELETE;
        upload_store /var/www/uploads;
    }

    location /cgi-bin {
        root /var/www;
        methods GET POST;
        cgi_interpreter /usr/bin/python3 .py;
        cgi_interpreter /bin/sh .sh;
    }
}
EOF
    echo "Default configuration saved to $file."
}

# --- Main Wizard ---

echo "========================================="
echo "   Server .conf Auto-Generator Wizard    "
echo "========================================="

# 1. Ask for output file
read -p "Enter output file name [server.conf]: " filename
filename=${filename:-server.conf}

# 2. Check for default mode
read -p "Do you want to generate the DEFAULT configuration? (y/n) [n]: " use_default
use_default=${use_default:-n}

if [[ "$use_default" =~ ^[Yy]$ ]]; then
    generate_default "$filename"
    exit 0
fi

echo -e "\nBuilding file..."

# NL the file
echo -e "\n\r\n\r" >> "$filename"

# Outer Loop for Multiple Server Blocks
while true; do
    echo -e "\n--- Configuring a Server Block ---"

    # Initialize server block
    echo "server {" >> "$filename"

    # 3. Server Directives
    read -p "Listen (e.g., 8080 or 127.0.0.1:8080) [8080]: " listen_val
    listen_val=${listen_val:-8080}
    echo "    listen $listen_val;" >> "$filename"

    read -p "Server Name (e.g., example.com) [localhost]: " server_name
    server_name=${server_name:-localhost}
    echo "    server_name $server_name;" >> "$filename"

    read -p "Client Max Body Size (e.g., 10M, 500K) [1M]: " max_body
    max_body=${max_body:-1M}
    echo "    client_max_body_size $max_body;" >> "$filename"

    # 4. Error Pages
    echo -e "\n-- Error Pages --"
    while true; do
        read -p "Add an error page? (y/n) [n]: " add_err
        [[ ! "$add_err" =~ ^[Yy]$ ]] && break

        read -p "  Error code (e.g., 404): " err_code
        read -p "  Error path (e.g., /errors/404.html): " err_path
        echo "    error_page $err_code $err_path;" >> "$filename"
    done

    # 5. Location Blocks
    echo -e "\n-- Location Blocks --"
    while true; do
        read -p "Add a location block? (y/n) [n]: " add_loc
        [[ ! "$add_loc" =~ ^[Yy]$ ]] && break

        read -p "  Location path (e.g., /api): " loc_path
        echo "" >> "$filename"
        echo "    location $loc_path {" >> "$filename"

        read -p "    Root path (leave empty to skip): " loc_root
        [[ -n "$loc_root" ]] && echo "        root $loc_root;" >> "$filename"

        read -p "    Allowed Methods (e.g., GET POST DELETE, empty to skip): " loc_methods
        [[ -n "$loc_methods" ]] && echo "        methods $loc_methods;" >> "$filename"

        read -p "    Autoindex (on/off, empty to skip): " loc_autoindex
        [[ -n "$loc_autoindex" ]] && echo "        autoindex $loc_autoindex;" >> "$filename"

        read -p "    Index file (e.g., index.html, empty to skip): " loc_index
        [[ -n "$loc_index" ]] && echo "        index $loc_index;" >> "$filename"

        read -p "    Upload store path (empty to skip): " loc_upload
        [[ -n "$loc_upload" ]] && echo "        upload_store $loc_upload;" >> "$filename"

        read -p "    Return redirect (e.g., 301 https://new-site.com, empty to skip): " loc_return
        [[ -n "$loc_return" ]] && echo "        return $loc_return;" >> "$filename"

        # CGI Interpreters inside location
        while true; do
            read -p "    Add a CGI interpreter to this location? (y/n) [n]: " add_cgi
            [[ ! "$add_cgi" =~ ^[Yy]$ ]] && break

            read -p "      Interpreter path (e.g., /usr/bin/python3, or leave empty for auto-detect): " cgi_path
            read -p "      Extension (e.g., .py): " cgi_ext

            # Format properly if path is empty
            if [[ -z "$cgi_path" ]]; then
                echo "        cgi_interpreter \"\" $cgi_ext;" >> "$filename"
            else
                echo "        cgi_interpreter $cgi_path $cgi_ext;" >> "$filename"
            fi
        done

        echo "    }" >> "$filename"
    done

    # Close current server block
    echo "}" >> "$filename"

    # 6. Ask to add another server block
    echo -e "\n-----------------------------------------"
    read -p "Would you like to add ANOTHER server block to this file? (y/n) [n]: " add_server
    if [[ ! "$add_server" =~ ^[Yy]$ ]]; then
        break
    else
        # Add a newline between server blocks for readability
        echo "" >> "$filename"
    fi

done

echo -e "\n========================================="
echo " Done! Configuration saved to: $filename"
echo "========================================="