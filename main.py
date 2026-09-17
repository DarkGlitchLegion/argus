import asyncio
from controller.listen import listen_bash_mode


def main():
    asyncio.run(listen_bash_mode())

if __name__ == "__main__":
    main()