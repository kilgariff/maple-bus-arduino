const int led_pin =  13;
const int data_pin_1 = 0;
const int data_pin_5 = 1;

int last_millis = 0;
int report_accumulator = 0;

volatile int d1_val = 0;
volatile int d5_val = 0;
volatile int millis_since_last_change = 0;
volatile int sync_d5_cycles = 0;
volatile int d1_changes = 0;
volatile int d5_changes = 0;
volatile bool did_start_syncing = false;
volatile bool did_start_phase1 = false;
volatile bool did_start_phase2 = false;
volatile int highest_sync_d5_cycles = 0;

//===============================================
void setup()
{
  pinMode(led_pin, OUTPUT);
  pinMode(data_pin_1, INPUT_PULLDOWN);
  pinMode(data_pin_5, INPUT_PULLDOWN);

  attachInterrupt(digitalPinToInterrupt(data_pin_1), d1_change, CHANGE);
  attachInterrupt(digitalPinToInterrupt(data_pin_5), d5_change, CHANGE);
  
  Serial.begin(9600);
  Serial.println("Finished setup");
  last_millis = millis();
}

//===============================================
enum State
{
  NoSignal,
  Idle,
  Syncing,
  Phase1,
  Phase2
};
volatile State state = NoSignal;

//===============================================

volatile uint8_t frame[4096];
volatile size_t data_idx = 0;
volatile size_t bit_idx = 0;
volatile size_t bits_read = 0;

//===============================================
void put_bit_in_frame(int val)
{
  frame[data_idx] |= ((val & 0x1) << bit_idx);
  ++bit_idx;
  ++bits_read;
  if (bit_idx == 8)
  {
    bit_idx = 0;
    ++data_idx;
    if (data_idx >= sizeof(frame))
    {
      // Wrap around to start (ring buffer)
      data_idx = 0;
    }
  }
}

//===============================================
void d1_change()
{
  d1_changes++;
  d1_val = digitalReadFast(data_pin_1);
  millis_since_last_change = 0;

  // If D1 has gone low while in idle state and
  // D5 is high, this is probably the start of
  // a sync.
  if (state == Idle &&
      d1_val == 0 &&
      d5_val == 1)
  {
    state = Syncing;
    sync_d5_cycles = 0;
    did_start_syncing = true;
  }

  else if (state == Syncing &&
           d1_val == 1
           && sync_d5_cycles != 4)
  {
    // In this case, it looks like we *should* have synced but
    // there weren't the right number of cycles.
    sync_d5_cycles = 0;
  }

  // Down flank in phase 1 means we commit a bit to our frame.
  else if (state == Phase1 && d1_val == 0)
  {
    put_bit_in_frame(d5_val);
    state = Phase2;
    did_start_phase2 = true;
  }
}

//===============================================
void d5_change()
{
  d5_changes++;
  d5_val = digitalReadFast(data_pin_5);
  millis_since_last_change = 0;

  if (state == Syncing)
  {
    if (d1_val == 0 && d5_val == 1)
    {
      ++sync_d5_cycles;
      if (sync_d5_cycles > highest_sync_d5_cycles)
      {
        highest_sync_d5_cycles = sync_d5_cycles;
      }
    }
    else if (d1_val == 1 && d5_val == 0 && sync_d5_cycles == 4)
    {
      // Finished syncing, enter phase 1.
      state = Phase1;
      did_start_phase1 = true;
    }
  }

  // Down flank in phase 2 means we commit a bit to our frame.
  else if (state == Phase2 && d5_val == 0)
  {
    put_bit_in_frame(d1_val);
    state = Phase1;
    did_start_phase1 = true;
  }
}

void loop()
{
  int current_millis = millis();
  int dt = current_millis - last_millis;
  last_millis = current_millis;

  // Detect Idle and No Signal states.
  millis_since_last_change += dt;
  if (millis_since_last_change >= 10)
  {
    if (d1_val == 1 && d5_val == 1)
    {
      state = Idle;
    }
    else if (d1_val == 0 && d5_val == 0)
    {
      state = NoSignal;
    }
  }

  // Report.
  report_accumulator += dt;
  if (report_accumulator >= 1000)
  {
    report_accumulator = 0;
    Serial.println("-----");

    Serial.print("State is ");

    switch(state)
    {
      case NoSignal:
      {
        Serial.println("No Signal");
        break;
      }

      case Idle:
      {
        Serial.println("Idle");
        break;
      }

      case Syncing:
      {
        Serial.println("Syncing");
        break;
      }

      case Phase1:
      {
        Serial.println("Phase1");
        break;
      }

      case Phase2:
      {
        Serial.println("Phase2");
        break;
      }
    }

    Serial.flush();

    Serial.print("Data 1: ");
    Serial.print(d1_val);
    Serial.print(" (changes: ");
    Serial.print(d1_changes);
    Serial.println(")");
    Serial.flush();
    d1_changes = 0;
    
    Serial.print("Data 5: ");
    Serial.print(d1_val);
    Serial.print(" (changes: ");
    Serial.print(d5_changes);
    Serial.println(")");
    Serial.flush();
    d5_changes = 0;

    Serial.print("Bits read: ");
    Serial.println(bits_read);
    Serial.print("KiB/s in: ");
    Serial.print((bits_read / 8) / 1024.0f);
    Serial.println("");
    Serial.flush();
    bits_read = 0;

    if (did_start_syncing)
    {
//      Serial.println("Started syncing at one point");
    }

    if (did_start_phase1)
    {
//      Serial.println("Entered phase1 at one point");
    }

    if (did_start_phase2)
    {
//      Serial.println("Entered phase2 at one point");
    }

    Serial.print("Highest sync d5 cycles was ");
    Serial.println(highest_sync_d5_cycles);
    highest_sync_d5_cycles = 0;

    did_start_syncing = false;
    did_start_phase1 = false;
    did_start_phase2 = false;
  }
}
