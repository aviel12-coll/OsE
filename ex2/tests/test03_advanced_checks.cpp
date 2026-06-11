#include "MapReduceJob.h"
#include <iostream>
#include <vector>
#include <memory>
#include <cassert>
#include <chrono>
#include <thread>

// אלמנט בסיסי המיועד לייצג מפתח וערך
class TestElement : public K1, public K2, public K3, public V1, public V2, public V3
{
public:
    int val;
    TestElement(int v) : val(v) {}

    // השוואה קריטית עבור std::sort ו-are_keys_equal
    bool operator<(const K1 &other) const override { return val < dynamic_cast<const TestElement&>(other).val; }
    bool operator<(const K2 &other) const override { return val < dynamic_cast<const TestElement&>(other).val; }
    bool operator<(const K3 &other) const override { return val < dynamic_cast<const TestElement&>(other).val; }
};

class AdvancedClient : public MapReduceClient
{
public:
    // שלב ה-Map: מייצר עומס קל ומפזר איברים
    void map(const std::shared_ptr<K1> key, const std::shared_ptr<V1> value,
              MapContext &context) const override
    {
        auto k = std::dynamic_pointer_cast<TestElement>(key);
        auto v = std::dynamic_pointer_cast<TestElement>(value);

        // מייצרים שני איברים ביניים כדי להגדיל את הוקטורים
        context.addIntermediate(k, v);
        context.addIntermediate(k, std::make_shared<TestElement>(v->val + 1));
    }

    // שלב ה-Reduce: מדמה עבודה אמיתית באמצעות השהייה קלה
    void reduce(const IntermediateVec &pairs, ReduceContext &context) const override
    {
        // האטה מכוונת כדי לאפשר ל-main לבדוק את הסטטוס בזמן שהתהליכונים עובדים
        std::this_thread::sleep_for(std::chrono::milliseconds(5));

        if (!pairs.empty())
        {
            auto k = std::dynamic_pointer_cast<TestElement>(pairs[0].first);
            context.addOutput(k, std::make_shared<TestElement>(static_cast<int>(pairs.size())));
        }
    }
};

int main()
{
    AdvancedClient client;
    InputVec inputVec;

    // 1. קלט גדול מספיק כדי לעקוב אחר אחוזי התקדמות
    for(int i = 0; i < 50; i++)
    {
        inputVec.push_back({std::make_shared<TestElement>(i % 5), std::make_shared<TestElement>(i)});
    }

    std::cout << "[TEST] Starting Job with 4 threads..." << std::endl;

    // יצירת הג'וב עם 4 תהליכונים
    MapReduceJob job(client, inputVec, 4);

    // 2. בדיקת דגימת סטטוס תוך כדי ריצה (בדיקת אטומיות ופרוגרסיביות)
    bool saw_mapping = false;
    bool saw_reducing = false;

    for (int i = 0; i < 20; i++)
    {
        MapReduceState state = job.getState();

        // ודאות שהאחוזים הגיוניים (בין 0 ל-100)
        assert(state.percentage >= 0.0 && state.percentage <= 100.0);

        if (state.stage == MAP_STAGE && state.percentage > 0) saw_mapping = true;
        if (state.stage == REDUCE_STAGE && state.percentage > 0) saw_reducing = true;

        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }

    std::cout << "[TEST] Testing job.wait()..." << std::endl;
    job.wait();

    // 3. בדיקת מצב סופי לאחר wait
    MapReduceState final_state = job.getState();
    std::cout << "[TEST] Final Stage: " << final_state.stage
              << " | Final Percentage: " << final_state.percentage << "%" << std::endl;

    // בודק קשוח יוודא שהגענו ל-100% בשלב ה-Reduce בסיום ה-wait
    assert(final_state.stage == REDUCE_STAGE);
    assert(final_state.percentage == 100.0);

    // בדיקה שפונקציית ה-isDone מחזירה אמת כשהעבודה באמת נגמרה
    if(!job.isDone())
    {
        std::cerr << "[FAIL] isDone() returned false after wait!" << std::endl;
        return 1;
    }

    // 4. בדיקת מניעת קריסה בריאה חוזרת של wait (Idempotency)
    try {
        job.wait();
    } catch (...) {
        std::cerr << "[FAIL] Call to wait() a second time caused an exception!" << std::endl;
        return 1;
    }

    std::cout << "[SUCCESS] All advanced checks passed successfully!" << std::endl;
    return 0;
}
