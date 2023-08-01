using System.Text;

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

        internal static ModeType GetUserModeSelection()
        {
            while (true)
            {
                // Print options
                WriteEmphasizedLine("MUCH SELECT (0-3):", ConsoleColor.Yellow);
                Console.WriteLine("0: LoRa Setup Mode");
                Console.WriteLine("1: SPV Mode");
                Console.WriteLine("2: Test Mode");
                Console.WriteLine($"Enter 'quit' to exit the program");

                string userInput = Console.ReadLine();
                if (String.Equals("quit", userInput, StringComparison.InvariantCultureIgnoreCase))
                {
                    return ModeType.Quit;
                }

                bool numParseSuccess = int.TryParse(userInput, out int selection);
                if (numParseSuccess && (selection == (int)ModeType.LoRaSetup || selection == (int)ModeType.SPV || selection == (int)ModeType.Test))
                {
                    return (ModeType)selection;
                }
                else
                {
                    WriteEmphasizedLine($"{userInput} is an invalid selection!", ConsoleColor.Red);
                }
            }
        }

        /// <summary>
        /// Print the received host command (byte array)
        /// </summary>
        /// <param name="commandToSend"></param>
        internal static void PrintCommandBytes(byte[] commandToSend)
        {
            Console.Write($"Host sent {commandToSend.Length - 1} bytes: ");
            for (int i = 0; i < commandToSend.Length; i++)
            {
                Console.Write(commandToSend[i].ToString("X2") + " ");
            }
            Console.WriteLine();
        }

        /// <summary>
        /// Print a provided payload in hexadecimal format
        /// </summary>
        /// <param name="payload"></param>
        internal static void PrintPayloadAsHex(byte[] payload)
        {
            StringBuilder hex = new StringBuilder(payload.Length * 2);
            foreach (byte b in payload)
            {
                hex.AppendFormat("{0:x2}", b);
            }
            Console.WriteLine(hex.ToString());
        }
    }
}
