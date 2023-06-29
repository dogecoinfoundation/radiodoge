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

        internal static ModeSelection GetUserModeSelection()
        {
            while (true)
            {
                // Print options
                WriteEmphasizedLine("MUCH SELECT (0-3):", ConsoleColor.Yellow);
                Console.WriteLine("0: LoRa Setup Mode");
                Console.WriteLine("1: Doge Mode");
                Console.WriteLine("2: SPV Mode");
                Console.WriteLine("3: Test Mode");
                Console.WriteLine($"Enter 'quit' to exit the program");

                string userInput = Console.ReadLine();
                if (String.Equals("quit", userInput, StringComparison.InvariantCultureIgnoreCase))
                {
                    return ModeSelection.Quit;
                }

                bool numParseSuccess = int.TryParse(userInput, out int selection);
                if (numParseSuccess && (selection == (int)ModeSelection.LoRaSetup || selection == (int)ModeSelection.Doge || selection == (int)ModeSelection.SPV || selection == (int)ModeSelection.Test))
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
