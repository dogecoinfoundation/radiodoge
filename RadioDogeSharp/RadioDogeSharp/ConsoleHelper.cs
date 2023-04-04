namespace RadioDoge
{
    internal static class ConsoleHelper
    {
        internal static void PrintTitleScreen()
        {
            string art = File.ReadAllText("asciiDoge.txt");
            WriteEmphasizedLine($"\n{art}\n\n", ConsoleColor.Yellow);
            WriteEmphasizedLine("### Welcome to RadioDoge, press enter to begin! ###", ConsoleColor.Cyan);
            Console.ReadLine();
        }

        internal static void WriteEmphasizedLine(string text, ConsoleColor consoleColor)
        {
            Console.ForegroundColor = consoleColor;
            Console.WriteLine(text);
            Console.ForegroundColor = ConsoleColor.White;
        }

        internal static void PrintSerialSetupCommandHelp()
        {
            WriteEmphasizedLine("Available LoRa Setup Commands:", ConsoleColor.Magenta);
            foreach (int i in Enum.GetValues(typeof(SerialCommandType)))
            {
                if (i < (int)'!' || i > (int)'z')
                {
                    WriteEmphasizedLine($"{i}: {(SerialCommandType)i}", ConsoleColor.Cyan);
                }
                else
                {
                    WriteEmphasizedLine($"{i} or {(char)i}: {(SerialCommandType)i}", ConsoleColor.Cyan);
                }
            }
            WriteEmphasizedLine("Enter 'exit' to return to the mode selection screen or 'help' for available commands\n", ConsoleColor.Magenta);
        }

        internal static void PrintDogeCommandHelp()
        {
            WriteEmphasizedLine("Available Doge Mode Commands:", ConsoleColor.Magenta);
            foreach (int i in Enum.GetValues(typeof(DogeCommands)))
            {
                WriteEmphasizedLine($"{i}: {(DogeCommands)i}", ConsoleColor.Cyan);
            }
            WriteEmphasizedLine("Enter 'exit' to return to the mode selection screen or 'help' for available commands\n", ConsoleColor.Magenta);
        }

        internal static ModeSelection GetUserModeSelection()
        {
            while (true)
            {
                // Print options
                WriteEmphasizedLine("MUCH SELECT (0-2):", ConsoleColor.Yellow);
                Console.WriteLine("0: LoRa Setup Mode");
                Console.WriteLine("1: Doge Mode");
                Console.WriteLine("2: Test Mode");
                Console.WriteLine($"Enter 'quit' to exit the program");

                string userInput = Console.ReadLine();
                if (String.Equals("quit", userInput, StringComparison.InvariantCultureIgnoreCase))
                {
                    return ModeSelection.Quit;
                }

                bool numParseSuccess = int.TryParse(userInput, out int selection);
                if (numParseSuccess && (selection == (int)ModeSelection.LoRaSetup || selection == (int)ModeSelection.Doge || selection == (int)ModeSelection.Test))
                {
                    return (ModeSelection)selection;
                }
                else
                {
                    WriteEmphasizedLine($"{userInput} is an invalid selection!", ConsoleColor.Red);
                }
            }
        }
    }
}
