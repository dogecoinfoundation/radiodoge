﻿using System;
using System.Numerics;
using System.Runtime.InteropServices;
using System.Text;

namespace RadioDoge
{
    public static class LibDogecoin
    {
        private const string libToImport = "dogecoin";
        private const int NUM_BYTES_PER_UTXO = 76;

        // @TODO review choice of types used
        // size_t -> UIntPtr
        // const char* -> string
        // uint8_t* -> IntPtr -> byte[]
        [DllImport(libToImport)]
        public static extern uint dogecoin_ecc_start();

        [DllImport(libToImport)]
        public static extern void dogecoin_ecc_stop();

        [DllImport(libToImport, CharSet = CharSet.Ansi, CallingConvention = CallingConvention.Cdecl)]
        public static extern int generatePrivPubKeypair(StringBuilder wif_privkey, StringBuilder p2pkh_pubkey, int testnet);

        [DllImport(libToImport, CharSet = CharSet.Ansi, CallingConvention = CallingConvention.Cdecl)]
        public static extern int generateHDMasterPubKeypair(StringBuilder wif_privkey_master, StringBuilder p2pkh_pubkey_master, bool is_testnet);

        [DllImport(libToImport, CharSet = CharSet.Ansi, CallingConvention = CallingConvention.Cdecl)]
        public static extern int generateDerivedHDPubkey(string wif_privkey_master, StringBuilder p2pkh_pubkey);

        [DllImport(libToImport, CharSet = CharSet.Ansi, CallingConvention = CallingConvention.Cdecl)]
        public static extern int verifyPrivPubKeypair(StringBuilder wif_privkey, StringBuilder p2pkh_pubkey, bool is_testnet);

        [DllImport(libToImport, CharSet = CharSet.Ansi, CallingConvention = CallingConvention.Cdecl)]
        public static extern int verifyHDMasterPubKeypair(StringBuilder wif_privkey_master, StringBuilder p2pkh_pubkey_master, bool is_testnet);

        [DllImport(libToImport, CharSet = CharSet.Ansi, CallingConvention = CallingConvention.Cdecl)]
        public static extern int verifyP2pkhAddress(StringBuilder p2pkh_pubkey, UIntPtr len);

        [DllImport(libToImport, CharSet = CharSet.Ansi, CallingConvention = CallingConvention.Cdecl)]
        public static extern int getDerivedHDAddress(string masterkey, UInt32 account, bool ischange, UInt32 addressindex, StringBuilder outaddress, bool outprivkey);

        [DllImport(libToImport, CharSet = CharSet.Ansi, CallingConvention = CallingConvention.Cdecl)]
        public static extern int getDerivedHDAddressByPath(string masterkey, string derived_path, StringBuilder outaddress, bool outprivkey);

        [DllImport(libToImport, CharSet = CharSet.Ansi, CallingConvention = CallingConvention.Cdecl)]
        private static extern int dogecoin_register_watch_address_with_node(string address);

        [DllImport(libToImport, CharSet = CharSet.Ansi, CallingConvention = CallingConvention.Cdecl)]
        private static extern int dogecoin_unregister_watch_address_with_node(string address);

        [DllImport(libToImport, CharSet = CharSet.Ansi, CallingConvention = CallingConvention.Cdecl)]
        private static extern IntPtr dogecoin_get_utxos(string address);

        [DllImport(libToImport, CharSet = CharSet.Ansi, CallingConvention = CallingConvention.Cdecl)]
        private static extern UInt32 dogecoin_get_utxos_length(string address);

        [DllImport(libToImport, CharSet = CharSet.Ansi, CallingConvention = CallingConvention.Cdecl)]
        private static extern IntPtr dogecoin_get_utxo_txid_str(string address, UInt32 index);

        [DllImport(libToImport, CharSet = CharSet.Ansi, CallingConvention = CallingConvention.Cdecl)]
        private static extern int dogecoin_get_utxo_vout(string address, UInt32 index);

        [DllImport(libToImport, CharSet = CharSet.Ansi, CallingConvention = CallingConvention.Cdecl)]
        private static extern IntPtr dogecoin_get_utxo_amount(string address, UInt32 index);

        [DllImport(libToImport, CharSet = CharSet.Ansi, CallingConvention = CallingConvention.Cdecl)]
        private static extern IntPtr dogecoin_get_utxo_txid(string address, UInt32 index);

        [DllImport(libToImport, CharSet = CharSet.Ansi, CallingConvention = CallingConvention.Cdecl)]
        private static extern UInt64 dogecoin_get_balance(string address);

        [DllImport(libToImport, CharSet = CharSet.Ansi, CallingConvention = CallingConvention.Cdecl)]
        private static extern IntPtr dogecoin_get_balance_str(string address);

        [DllImport(libToImport, CharSet = CharSet.Ansi, CallingConvention = CallingConvention.Cdecl)]
        private static extern int koinu_to_coins_str(UInt64 koinu_amount, StringBuilder amount_string);

        [DllImport(libToImport, CharSet = CharSet.Ansi, CallingConvention = CallingConvention.Cdecl)]
        private static extern UInt64 coins_to_koinu_str(string coins);

        [DllImport(libToImport, CharSet = CharSet.Ansi, CallingConvention = CallingConvention.Cdecl)]
        private static extern byte dogecoin_p2pkh_address_to_pubkey_hash(string address, StringBuilder pubkeyResult);

        [DllImport(libToImport, CharSet = CharSet.Ansi, CallingConvention = CallingConvention.Cdecl)]
        private static extern IntPtr chain_from_b58_prefix(string address);

        [DllImport(libToImport, CharSet = CharSet.Ansi, CallingConvention = CallingConvention.Cdecl)]
        private static extern bool broadcast_raw_tx(IntPtr chainParameters, string rawTransaction);

        [DllImport(libToImport, CharSet = CharSet.Ansi, CallingConvention = CallingConvention.Cdecl)]
        private static extern IntPtr broadcast_raw_tx_on_net(string raw_hex_tx, byte is_testnet);

        /// <summary>
        /// Get Dogecoin balance in Koinu for the specified address
        /// </summary>
        /// <param name="address"></param>
        /// <returns></returns>
        public static UInt64 GetBalance(string address)
        {
            return dogecoin_get_balance(address);
        }

        /// <summary>
        /// Get Dogecoin balance in coins for the specified address
        /// </summary>
        /// <param name="address"></param>
        /// <returns></returns>
        public static string GetBalanceString(string address)
        {
            IntPtr balanceStringPointer = dogecoin_get_balance_str(address);
            return Marshal.PtrToStringAnsi(balanceStringPointer);
        }

        /// <summary>
        /// Get the number of UTXOs for the specified Dogecoin address
        /// </summary>
        /// <param name="address"></param>
        /// <returns></returns>
        public static UInt32 GetNumberOfUTXOs(string address)
        {
            return dogecoin_get_utxos_length(address);
        }

        /// <summary>
        /// Get all of the UTXOs for the specified dogecoin address
        /// </summary>
        /// <param name="numUTXOs"></param>
        /// <param name="address"></param>
        /// <returns></returns>
        public static UnspentTransactionOutput[] GetAllUTXOs(UInt32 numUTXOs, string address)
        {
            if (numUTXOs > 0)
            {
                UnspentTransactionOutput[] outputs = new UnspentTransactionOutput[numUTXOs];
                string currTXID;
                string currAmountString;
                UInt64 currAmount;
                int currVout;
                for (uint i = 0; i < numUTXOs; i++)
                {
                    IntPtr txidStringPointer = dogecoin_get_utxo_txid_str(address, i + 1);
                    currTXID = Marshal.PtrToStringAnsi(txidStringPointer);
                    IntPtr amountStringPointer = dogecoin_get_utxo_amount(address, i + 1);
                    currAmountString = Marshal.PtrToStringAnsi(amountStringPointer);
                    currAmount = coins_to_koinu_str(currAmountString);
                    currVout = dogecoin_get_utxo_vout(address, i + 1);
                    outputs[i] = new UnspentTransactionOutput(currVout, currTXID, currAmount);
                    Console.WriteLine(outputs[i].ToString() + "\n");
                }
                return outputs;
            }
            else
            {
                return null;
            }
        }

        public static byte[] GetAllSerializedUTXOs(UInt32 numUTXOs, string address)
        {
            UnspentTransactionOutput[] outputs = GetAllUTXOs(numUTXOs, address);
            if (outputs == null)
            {
                return null;
            }
            List<byte> serializedOutputs = new List<byte>();
            for (int i = 0; i < outputs.Length; i++)
            {
                serializedOutputs.AddRange(outputs[i].Serialize());
            }
            return serializedOutputs.ToArray();
        }

        /// <summary>
        /// Get the TXID of a specific UTXO for the specified Dogecoin address
        /// </summary>
        /// <param name="address"></param>
        /// <param name="index"></param>
        /// <returns></returns>
        /// <exception cref="ArgumentException"></exception>
        public static string GetTXIDString(string address, uint index)
        {
            // Indexing begins at 1
            if (index < 1)
            {
                throw new ArgumentException("Invalid index");
            }

            IntPtr txidStringPointer = dogecoin_get_utxo_txid_str(address, index);
            return Marshal.PtrToStringAnsi(txidStringPointer);
        }

        /// <summary>
        /// Convert a Koinu balance (UInt64) into a balance in coins (string)
        /// </summary>
        /// <param name="koinuAmount"></param>
        /// <param name="coinString"></param>
        /// <returns></returns>
        public static bool ConvertKoinuAmountToString(UInt64 koinuAmount, out string coinString)
        {
            StringBuilder resultString = new StringBuilder();
            int resultCode = koinu_to_coins_str(koinuAmount, resultString);
            coinString = resultString.ToString();
            return resultCode == 1;
        }

        public static bool GetPubKeyHash(string address, out string pubkeyHash)
        {
            StringBuilder pubkey = new StringBuilder();
            byte result = dogecoin_p2pkh_address_to_pubkey_hash(address, pubkey);
            pubkeyHash = pubkey.ToString();
            Console.WriteLine($"PubKey Hash: {pubkeyHash}");
            return result == 1;
        }

        /// <summary>
        /// Broadcast a raw transaction on the Dogecoin network
        /// </summary>
        /// <param name="rawTransaction"></param>
        /// <returns></returns>
        public static string BroadcastTransaction(string rawTransaction)
        {
            byte dogecoin_bool = 0;
            IntPtr stringPtr = broadcast_raw_tx_on_net(rawTransaction, dogecoin_bool);
            return Marshal.PtrToStringAnsi(stringPtr);
        }

        public static bool RegisterWatchAddress(string addressToRegister)
        {
            int returnCode = dogecoin_register_watch_address_with_node(addressToRegister);
            return returnCode == 1;
        }

        public static bool UnregisterWatchAddress(string addressToUnregister)
        {
            int returnCode = dogecoin_unregister_watch_address_with_node(addressToUnregister);
            return returnCode == 1;
        }
    }
}




