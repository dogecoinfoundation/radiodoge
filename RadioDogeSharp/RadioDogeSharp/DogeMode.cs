﻿using System;
using System.Diagnostics;
using System.Text;

namespace RadioDoge
{
    public partial class SerDogeSharp
    {
        private Dictionary<string, AddressBookEntry> dogeAddressBook = new Dictionary<string, AddressBookEntry>();
        private const int PIN_LENGTH = 4;
        private const int MAX_ADDRESS_LENGTH = 35;
        private string[] testAddresses = new string[] { "D6JQ6C48u9yYYarubpzdn2tbfvEq12vqeY", "DBcR32NXYtFy6p4nzSrnVVyYLjR42VxvwR", "DGYrGxANmgjcoZ9xJWncHr6fuA6Y1ZQ56Y" };

        private void ProcessDogePayload(NodeAddress senderAddress, byte[] payload)
        {
            DogeCommandType commType = (DogeCommandType)payload[0];
            switch (commType)
            {
                case DogeCommandType.SendDogeAddress:
                    // This means we got sent a dogecoin address
                    int addrLength = payload.Length - 1;
                    if (addrLength <= MAX_ADDRESS_LENGTH)
                    {
                        string addressString = ExtractDogecoinAddressFromPayload(1, addrLength, payload);
                        Console.WriteLine($"Received Dogecoin Address: {addressString}");
                    }
                    else
                    {
                        Console.WriteLine("Received an address that was too long!");
                        SendGenericDogeResponse(senderAddress, false, DogeCommandType.SendDogeAddress, DogeResponseCode.InvalidAddress);
                    }
                    break;
                case DogeCommandType.GetDogeAddress:
                    // This means someone is requesting a dogecoin address so we would send ours out
                    SendDogeAddress(senderAddress);
                    break;
                case DogeCommandType.RequestBalance:
                    ServiceBalanceRequest(senderAddress, payload);
                    break;
                case DogeCommandType.Registration:
                    RegisterDogeAddress(senderAddress, payload);
                    break;
                case DogeCommandType.GetUTXOs:
                    Console.WriteLine("Servicing UTXO Request!");
                    ServiceUTXORequest(senderAddress, payload);
                    break;
                case DogeCommandType.TransactionRequest:
                    Console.WriteLine("Servicing Transaction Request!");
                    ServiceTransactionRequest(senderAddress, payload);
                    break;
                case DogeCommandType.BroadcastReceived:
                    Console.WriteLine("Broadcast received!");
                    ProcessReceivedBroadcast(senderAddress, payload);
                    break;
                default:
                    Console.WriteLine("Unknown doge payload. Raw data:");
                    ConsoleHelper.PrintPayloadAsHex(payload);
                    break;
            }
        }

        private void SendDogeCommand(int commandValue)
        {
            DogeCommandType dogeCommandType = (DogeCommandType)commandValue;
            ConsoleHelper.WriteEmphasizedLine($"Sending Command: {dogeCommandType}", ConsoleColor.Yellow);
            switch (dogeCommandType)
            {
                case DogeCommandType.GetDogeAddress:
                    RequestDogeCoinAddress(destinationAddress);
                    return;
                case DogeCommandType.RequestBalance:
                    RequestDogeCoinBalance(destinationAddress, Encoding.ASCII.GetBytes(testAddresses[0]));
                    return;
                default:
                    ConsoleHelper.WriteEmphasizedLine("Unknown command", ConsoleColor.Red);
                    break;
            }
        }

        private void ProcessReceivedBroadcast(NodeAddress senderAddress, byte[] payload)
        {
            if (payload.Length < 2)
            {
                ConsoleHelper.WriteEmphasizedLine("ERROR: Broadcast packet was malformed!", ConsoleColor.Red);
                return;
            }

            BroadCastType broadcastType = (BroadCastType)payload[1];
            switch(broadcastType)
            {
                case BroadCastType.HubAnnouncement:
                    Console.WriteLine($"Broadcasted Hub Address: {senderAddress}");
                    break;
                case BroadCastType.NodeAnnouncement:
                    Console.WriteLine($"Broadcasted Node Address: {senderAddress}");
                    break;
                default:
                    Console.WriteLine("Unknown broadcast type!");
                    break;
            }
        }

        /// <summary>
        /// Send UTXOs for the specified Dogecoin address to a another node
        /// </summary>
        /// <param name="destNode"></param>
        /// <param name="address"></param>
        private void SendUTXOs(NodeAddress destNode, string address)
        {
            UInt32 numUTXOs = LibDogecoin.GetNumberOfUTXOs(address);
            if (numUTXOs > 0)
            {
                byte[] serializedUTXOBytes = LibDogecoin.GetAllSerializedUTXOs(numUTXOs, address);
                // 1 byte for type, 4 for serialized num UTXO length, rest is for utxos
                List<byte> payload = new List<byte>(5 + serializedUTXOBytes.Length);
                payload.Add((byte)DogeCommandType.SendUTXOs);
                byte[] serializedLength = BitConverter.GetBytes(numUTXOs);
                payload.AddRange(serializedLength);
                payload.AddRange(serializedUTXOBytes);
                portManager.SendMultipartPacket(localAddress, destNode, payload.ToArray());
            }
            else
            {
                Console.WriteLine($"No UTXOs for {address} found!");
                SendGenericDogeResponse(destNode, false, DogeCommandType.GetUTXOs, DogeResponseCode.NoStoredUTXOS);
            }
        }

        private void ServiceUTXORequest(NodeAddress replyAddress, byte[] payload)
        {
            int addressLength = payload.Length - 1;
            string requestedDogecoinAddress = ExtractDogecoinAddressFromPayload(1, addressLength, payload);
            if (dogeAddressBook.ContainsKey(requestedDogecoinAddress))
            {
                SendUTXOs(replyAddress, requestedDogecoinAddress);
            }
            else
            {
                // Address book does not contain requested address
                // We will send a failure message for now
                SendGenericDogeResponse(replyAddress, false, DogeCommandType.GetUTXOs, DogeResponseCode.NotRegistered);
            }
        }

        private void ServiceTransactionRequest(NodeAddress replyAddress, byte[] payload)
        {
            // Extract transaction from payload
            int transactionLength = payload.Length - 1;
            char[] rawTransaction = new char[transactionLength];
            Array.Copy(payload, 1, rawTransaction, 0, transactionLength);
            string transactionString = new string(rawTransaction);
            Console.WriteLine($"Raw Transaction: {transactionString}");

            // Send out transaction
            //RunDogecoinSendTXCommand(transactionString);           
            Console.WriteLine("Sending transaction...");
            string transactionId = LibDogecoin.BroadcastTransaction(transactionString);
            Console.WriteLine($"TXID: {transactionId}");

            // Send back the txid as a receipt (0 if failure)
            SendTransactionResult(replyAddress, transactionId);
        }

        private void ServiceBalanceRequest(NodeAddress replyAddress, byte[] payload)
        {
            int addressLength = payload.Length - 1;
            string requestAddress = ExtractDogecoinAddressFromPayload(1, addressLength, payload);
            if (dogeAddressBook.ContainsKey(requestAddress))
            {
                // Get the balance
                UInt64 testBalance = LibDogecoin.GetBalance(requestAddress);

                // Modify it with the pin
                byte[] pin = dogeAddressBook[requestAddress].GetPin();
                byte[] balanceToSend = ObfuscateDogeCoinBalance(testBalance, pin);

                // Printing for debug purposes REMOVE LATER
                UInt64 obfuscatedValue = BitConverter.ToUInt64(balanceToSend, 0);
                Console.WriteLine($"Balance: {testBalance}, Obfuscated: {obfuscatedValue}");

                // Send out obfuscated balance
                SendDogeCoinBalance(replyAddress, balanceToSend);
            }
            else
            {
                // Address book does not contain requested address
                // We will send a failure message for now
                SendGenericDogeResponse(replyAddress, false, DogeCommandType.RequestBalance, DogeResponseCode.NotRegistered);
            }
        }

        private string ExtractDogecoinAddressFromPayload(int offsetIndex, int addrLength, byte[] payload)
        {
            char[] dogecoinAddress = new char[addrLength];
            Array.Copy(payload, offsetIndex, dogecoinAddress, 0, addrLength);
            return new string(dogecoinAddress);
        }

        private void RegisterDogeAddress(NodeAddress sender, byte[] payload)
        {
            // First get the registration function to be performed
            RegistrationFunction regOpcode = (RegistrationFunction)payload[1];
            int addressLength = payload.Length;
            if (regOpcode == RegistrationFunction.UpdatePin)
            {
                addressLength -= (2 * PIN_LENGTH) + 2;
            }
            else
            {
                addressLength -= PIN_LENGTH + 2;
            }

            // Initialized response variables
            bool operationSuccess = false;
            DogeResponseCode responseCode = DogeResponseCode.Success;

            // First check if address is valid
            if (addressLength <= MAX_ADDRESS_LENGTH)
            {
                string addressString = ExtractDogecoinAddressFromPayload(2, addressLength, payload);
                // Extract pin from payload
                byte[] extractedPin = new byte[PIN_LENGTH];
                Array.Copy(payload, addressLength + 2, extractedPin, 0, PIN_LENGTH);
                switch (regOpcode)
                {
                    case RegistrationFunction.UpdatePin:
                        if (dogeAddressBook.ContainsKey(addressString))
                        {
                            // Check if old pin matches new pin
                            byte[] updatedPin = new byte[PIN_LENGTH];
                            Array.Copy(payload, addressLength + 2 + PIN_LENGTH, updatedPin, 0, PIN_LENGTH);
                            bool pinUpdateSuccess = dogeAddressBook[addressString].ResetPin(extractedPin, updatedPin);
                            if (pinUpdateSuccess)
                            {
                                Console.WriteLine($"Pin successfully updated for {addressString}");
                                operationSuccess = true;
                            }
                            else
                            {
                                Console.WriteLine($"Pin could not be updated for {addressString}");
                                responseCode = DogeResponseCode.InvalidPin;
                            }
                        }
                        else
                        {
                            Console.WriteLine("Address has not been previously registered!");
                            responseCode = DogeResponseCode.NotRegistered;
                        }
                        break;
                    case RegistrationFunction.AddRegistration:
                        bool alreadyRegistered = dogeAddressBook.ContainsKey(addressString);
                        if (!alreadyRegistered)
                        {
                            Console.WriteLine($"Registering Dogecoin Address: {addressString}");
                            string pinString = GetStringFromPin(extractedPin);
                            Console.WriteLine($"Pin: {pinString}");
                            AddressBookEntry entry = new AddressBookEntry(sender, extractedPin);
                            dogeAddressBook.Add(addressString, entry);
                            operationSuccess = true;
                            bool addWatchSuccess = LibDogecoin.RegisterWatchAddress(addressString);
                            if (addWatchSuccess)
                            {
                                Console.WriteLine("Successfully added to address to watchlist!");
                            }
                            else
                            {
                                Console.WriteLine("Error: Failed to add address to watchlist!");
                                responseCode = DogeResponseCode.WatchlistFailure;
                            }
                        }
                        else
                        {
                            Console.WriteLine("Address already registered!");
                            responseCode = DogeResponseCode.AlreadyRegistered;
                        }
                        break;
                    case RegistrationFunction.RemoveRegistration:
                        // Check if the address is registered first
                        if (dogeAddressBook.ContainsKey(addressString))
                        {
                            // Check if pin matches
                            if (dogeAddressBook[addressString].DoesPinMatch(extractedPin))
                            {
                                dogeAddressBook.Remove(addressString);
                                Console.WriteLine($"Address {addressString} successfully removed!");
                                operationSuccess = true;
                                bool removeWatchSuccess = LibDogecoin.UnregisterWatchAddress(addressString);
                                if (removeWatchSuccess)
                                {
                                    Console.WriteLine("Successfully removed address from watchlist!");
                                }
                                else
                                {
                                    Console.WriteLine("Error: Failed to remove address from watchlist!");
                                    responseCode = DogeResponseCode.WatchlistFailure;
                                }
                            }
                            else
                            {
                                Console.WriteLine($"Pin does not match for {addressString}. Unable to remove registration!");
                                responseCode = DogeResponseCode.InvalidPin;
                            }
                        }
                        else
                        {
                            responseCode = DogeResponseCode.NotRegistered;
                        }
                        break;
                }
            }
            else
            {
                Console.WriteLine($"Failed to perform registration function: {regOpcode}");
                responseCode = DogeResponseCode.InvalidAddress;
            }

            // Send response back to the sender
            SendGenericDogeResponse(sender, operationSuccess, DogeCommandType.Registration, responseCode);

            // Print address book for debugging purposes
            Console.WriteLine();
            PrintDogeAddressBook();
        }

        private byte[] ObfuscateDogeCoinBalance(UInt64 balance, byte[] pin)
        {
            byte[] serializedBalance = BitConverter.GetBytes(balance);
            for (int i = 0; i < serializedBalance.Length; i++)
            {
                serializedBalance[i] ^= pin[i % PIN_LENGTH];
            }
            return serializedBalance;
        }

        private float DeobfuscateDogeCoinBalance(byte[] serializedBalance, byte[] pin)
        {
            for (int i = 0; i < serializedBalance.Length; i++)
            {
                serializedBalance[i] ^= pin[i];
            }
            return BitConverter.ToSingle(serializedBalance, 0);
        }

        private string GetStringFromPin(byte[] pin)
        {
            string pinString = "";
            for (int i = 0; i < PIN_LENGTH; i++)
            {
                pinString += pin[i].ToString();
            }
            return pinString;
        }

        private void PrintDogeCommandHelp()
        {
            ConsoleHelper.WriteEmphasizedLine("Available Doge Mode Commands:", ConsoleColor.Magenta);
            foreach (int i in Enum.GetValues(typeof(DogeCommandType)))
            {
                ConsoleHelper.WriteEmphasizedLine($"{i}: {(DogeCommandType)i}", ConsoleColor.Cyan);
            }
            ConsoleHelper.WriteEmphasizedLine("Enter 'exit' to return to the mode selection screen or 'help' for available commands\n", ConsoleColor.Magenta);
        }

        private void PrintDogeAddressBook()
        {
            Console.WriteLine("### Dogecoin Address Book ###");
            Dictionary<string, AddressBookEntry>.KeyCollection keys = dogeAddressBook.Keys;
            foreach (string dogeAddress in keys)
            {
                Console.WriteLine($"Address: {dogeAddress}, Node: {dogeAddressBook[dogeAddress].GetNodeAddress()}");
            }
        }

        /// <summary>
        /// No longer needed with new libdogecoin function that exposes the broadcast transaction function
        /// </summary>
        /// <param name="transaction"></param>
        private void RunDogecoinSendTXCommand(string transaction)
        {
            Console.WriteLine("Running SendTX...");
            ProcessStartInfo startInfo = new ProcessStartInfo
            {
                FileName = "sendtx",
                RedirectStandardInput = true,
                RedirectStandardOutput = false,
                UseShellExecute = false,
                CreateNoWindow = false,
                Arguments = transaction
            };
            Process process = new Process { StartInfo = startInfo };

            process.Start();
            while (Console.ReadLine() != "exit")
            {
                Console.WriteLine("Waiting for 'exit'...");
                Thread.Sleep(1000);
            }
            process.StandardInput.Close();
            process.Kill(true);
            process.Close();
            Console.WriteLine("Quitting Send TX");
        }     
    }
}
