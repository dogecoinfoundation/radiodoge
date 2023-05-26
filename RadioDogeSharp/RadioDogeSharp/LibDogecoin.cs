using System.Numerics;
using System.Runtime.InteropServices;
using System.Text;

namespace RadioDoge
{
    public static class LibDogecoin
    {
        private const string libToImport = "dogecoin";
        private const int NUM_BYTES_PER_UTXO = 55; // Should this be 41

        /* This is what makes up one UTXO so should be serialized to 35 bytes
         uint8_t index; // 1 byte
         uint16_t amount; // should this be uint64_t???
         uint8_t txid[32]
        */

        public static void DogeTest()
        {
            Console.WriteLine("Testing DogeCoin Library...");
            string privatekey;
            string publickey;

            int len = 256;
            StringBuilder pvkey = new StringBuilder(len);
            StringBuilder pubkey = new StringBuilder(len);
            dogecoin_ecc_start();
            int successret = generatePrivPubKeypair(pvkey, pubkey, 0);

            if (successret == 1)
            {
                privatekey = pvkey.ToString();
                publickey = pubkey.ToString();

                Console.WriteLine("Generated test address info:");
                Console.WriteLine($"Private key: {privatekey}");
                Console.WriteLine($"Public key: {publickey}");
            }

            dogecoin_ecc_stop();
            Console.WriteLine("Dogecoin library test successful!\n");
        }
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
        public static extern int dogecoin_register_watch_address_with_node(string address);

        [DllImport(libToImport, CharSet = CharSet.Ansi, CallingConvention = CallingConvention.Cdecl)]
        public static extern int dogecoin_unregister_watch_address_with_node(string address);

        /*
        [DllImport(libToImport, CharSet = CharSet.Ansi, CallingConvention = CallingConvention.Cdecl)]
        public static extern int dogecoin_get_utxo_vector(string address, vector* utxos);
        */

        [DllImport(libToImport, CharSet = CharSet.Ansi, CallingConvention = CallingConvention.Cdecl)]
        private static extern IntPtr dogecoin_get_utxos(string address);

        [DllImport(libToImport, CharSet = CharSet.Ansi, CallingConvention = CallingConvention.Cdecl)]
        private static extern UInt32 dogecoin_get_utxos_length(string address);

        [DllImport(libToImport, CharSet = CharSet.Ansi, CallingConvention = CallingConvention.Cdecl)]
        private static extern IntPtr dogecoin_get_utxo_txid_str(string address, UInt32 index);

        [DllImport(libToImport, CharSet = CharSet.Ansi, CallingConvention = CallingConvention.Cdecl)]
        private static extern IntPtr dogecoin_get_utxo_txid(string address, UInt32 index);

        [DllImport(libToImport, CharSet = CharSet.Ansi, CallingConvention = CallingConvention.Cdecl)]
        private static extern UInt64 dogecoin_get_balance(string address);

        [DllImport(libToImport, CharSet = CharSet.Ansi, CallingConvention = CallingConvention.Cdecl)]
        private static extern IntPtr dogecoin_get_balance_str(string address);

        [DllImport(libToImport, CharSet = CharSet.Ansi, CallingConvention = CallingConvention.Cdecl)]
        private static extern int koinu_to_coins_str(UInt64 koinu_amount, StringBuilder amount_string);

        public static UInt64 GetBalance(string address)
        {
            return dogecoin_get_balance(address);
        }

        public static string GetBalanceString(string address)
        {
            IntPtr balanceStringPointer = dogecoin_get_balance_str(address);
            return Marshal.PtrToStringAnsi(balanceStringPointer);
        }

        public static UInt32 GetNumberOfUTXOs(string address)
        {
            return dogecoin_get_utxos_length(address);
        }

        public static byte[] GetAllSerializedUTXOs(UInt32 numUTXOs, string address)
        {
            if (numUTXOs > 0 )
            {
                // Testing out getting serialized utxos
                IntPtr utxosPointer = dogecoin_get_utxos(address);
                int serializedLength = (int)numUTXOs * NUM_BYTES_PER_UTXO;
                byte[] serializedUTXOs = new byte[serializedLength];
                Marshal.Copy(utxosPointer, serializedUTXOs, 0, serializedLength);
                return serializedUTXOs;
            }
            else
            {
                return null;
            }
        }

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

        public static bool ConvertKoinuAmountToString(UInt64 koinuAmount, out string coinString)
        {
            StringBuilder resultString = new StringBuilder();
            int resultCode = koinu_to_coins_str(koinuAmount, resultString);
            coinString = resultString.ToString();
            return resultCode == 1;
        }
    }
}




