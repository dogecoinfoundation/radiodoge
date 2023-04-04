namespace RadioDoge
{
    internal static class ConsoleHelper
    {
        internal static void PrintTitleScreen()
        {
            string art = File.ReadAllText("asciiDoge.txt");
            Console.WriteLine($"\n{art}\n\n");
            WriteEmphasizedLine("### Welcome to RadioDoge, press enter to begin! ###", ConsoleColor.Cyan);
            Console.ReadLine();
        }

        internal static void WriteEmphasizedLine(string text, ConsoleColor consoleColor)
        {
            Console.ForegroundColor = consoleColor;
            Console.WriteLine(text);
            Console.ForegroundColor = ConsoleColor.White;
        }

        internal static void PrintSerialCommandHelp()
        {
            WriteEmphasizedLine("Available Commands:", ConsoleColor.Magenta);
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
            WriteEmphasizedLine("Enter 'quit' to exit or 'help' for available commands\n", ConsoleColor.Magenta);
        }
    }
}
