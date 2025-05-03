# <img src="icon.png" alt="X-2R Logo" width="50" height="50"/> X-2R

A Discord-like chat application built in C as a learning project.

## 🚀 About This Project

X-2R is a desktop chat application developed primarily in C. The main goal of this project was to explore and learn various concepts including:

*   **Network Programming:** Implementing client-server communication using sockets (Winsock on Windows, BSD sockets on Unix-like systems).
*   **Database Interaction:** Connecting to and managing data in a PostgreSQL database using the `libpq` library.
*   **Basic Security:** Experimenting with XOR encryption for password handling (Note: XOR is **not** secure for real-world password storage and is used here purely for educational purposes).
*   **Environment Management:** Utilizing `.env` files (`.env.server` for the server, `.env.client` for the client) to manage configuration settings like database credentials.
*   **GUI Development:** Building the user interface with GTK3.

The application mimics some basic functionalities found in platforms like Discord, such as user authentication, channels, and real-time messaging.

## ✨ Features

*   User Registration and Login
*   Multiple Chat Channels (public/private concepts, though implementation might vary)
*   Real-time Messaging within Channels
*   Display of Online Users (basic status)
*   Database Persistence for Users, Channels, and Messages

## 🛠️ Tech Stack

*   **Language:** C
*   **GUI:** GTK3
*   **Database:** PostgreSQL
*   **Database Client Library:** libpq
*   **Networking:** Winsock (Windows) / BSD Sockets (Unix-like)
*   **Build System:** CMake (implied, common for C projects)
*   **Environment Variables:** Custom loader for `.env` files

## ⚙️ Setup & Installation

### Prerequisites

*   A C Compiler (like GCC or Clang)
*   CMake
*   GTK3 Development Libraries
*   PostgreSQL Server and Development Libraries (`libpq-dev` or equivalent)

### Building from Source

1.  **Clone the repository:**
    ```bash
    git clone <your-repo-url>
    cd X-2R
    ```
2.  **Configure Environment Variables:**
    *   Create a `.env.server` file in the server's execution directory with your PostgreSQL connection details:
        ```dotenv
        PG_HOST=localhost
        PG_PORT=5432
        PG_DB=db_discord
        PG_USER=your_db_user
        PG_PASSWORD=your_db_password
        ```
    *   Create a `.env.client` file (if needed by the client for specific settings, otherwise server details might be hardcoded or fetched differently).
3.  **Setup Database:**
    *   Create a PostgreSQL database (e.g., `db_discord`).
    *   Run the `src/database/db_discord.sql` script to create the necessary tables.
4.  **Build the project:**
    ```bash
    mkdir build
    cd build
    cmake ..
    make
    ```
    The server and client executables should be in the `build` directory (or a subdirectory like `build/src`).
5.  **Run the Server:**
    ```bash
    ./server_executable # Adjust path as needed
    ```
6.  **Run the Client:**
    ```bash
    ./client_executable # Adjust path as needed
    ```

### Windows Executable

A pre-compiled `.exe` file for the client might be available for direct download and installation on Windows systems (check releases). Ensure the server is running and accessible.

## 👥 Contributors

*   **Ryhad B.** - [@RyderBlack](https://github.com/RyderBlack)
*   **Rimma C.** - [@RimmaChukhno](https://github.com/RimmaChukhno)

## 🏢 Organization

*   **La Plateforme_** - [Website](https://laplateforme.io/)

## 📄 License

[Specify your license here, e.g., MIT License]

---

*This project was developed as part of an educational program at La Plateforme_.* 